/**
 * Copyright Martin Raiber. All Rights Reserved.
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */
#pragma once

#include "SingleFileStorage.h"
#include "ThreadSafety.h"
#include <array>
#include <atomic>
#include <condition_variable>
#include <folly/SharedMutex.h>
#include <functional>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <string>
#include <thread>
#include <vector>

class StreamedValue;
struct ContentType;
class ObjectReader;

// Lock with two groups: Members of the same group hold it concurrently, members of different groups exclude each other.
// Fair: New members only enter directly if the other group is neither active nor waiting. Otherwise they wait until
// the active members of the other group are done, which then admits all members waiting at that time
class GroupMutex
{
public:
    void lock(const int group)
    {
        const int other = 1 - group;
        std::unique_lock lock(mutex);
        if (active[other] == 0 && waiting[other] == 0)
        {
            ++active[group];
            return;
        }
        ++waiting[group];
        const auto myBatch = batch[group];
        // Admitted members still wait if the other group entered again (before they woke up)
        cond.wait(lock, [&]()
                  { return active[other] == 0 && (batch[group] != myBatch || waiting[other] == 0); });
        --waiting[group];
        ++active[group];
    }

    void unlock(const int group)
    {
        const int other = 1 - group;
        std::scoped_lock lock(mutex);
        if (--active[group] == 0 && waiting[other] > 0)
        {
            // Admit the members of the other group waiting now
            ++batch[other];
            cond.notify_all();
        }
    }

private:
    std::mutex mutex;
    std::condition_variable cond;
    int active[2] = { 0, 0 };
    int waiting[2] = { 0, 0 };
    // Incremented to admit the waiting members of a group
    uint64_t batch[2] = { 0, 0 };
};

// Value argument of a command. Either in memory or streamed to the data file while the command was received
struct ValueArg
{
    std::string_view data;
    std::shared_ptr<StreamedValue> streamed;
};

/**
 * Protocol independent data layer: Keys stored as objects in a versioned bucket (see RedisServer for the data model),
 * with strings, lists, hashes, sets, sorted sets, key expiry and locking. Used by the protocol front-ends (Redis).
 * Errors are returned as RESP error replies.
 */
class KeyStore
{
public:
    KeyStore(SingleFileStorage& sfs, const std::string& bucket);
    ~KeyStore();

    KeyStore(const KeyStore&) = delete;
    KeyStore& operator=(const KeyStore&) = delete;

    // Lets the storage engine count list elements in the bucket (buckets::BucketInfo::redis). Has to be set up before
    // the storage engine is started (to count modifications replayed from the WAL)
    static void setupCounters(SingleFileStorage::SFSOptions& options);

    // Creates the bucket (with versioning enabled) if it does not exist yet and marks it as Redis bucket
    static bool initBucket(const std::string& bucket);

    // Starts the background expiry
    void start() EXCLUDES(expiryThreadMutex);
    void stop() EXCLUDES(expiryThreadMutex);

    enum class KeyType
    {
        None,
        String,
        List,
        Hash,
        Set,
        ZSet
    };

    // Expiry of a key
    struct ExpiryInfo
    {
        // Expiry time (ms since epoch)
        int64_t ms;
        // Version of the entry in the expiry index
        int64_t indexVersion;

        bool operator==(const ExpiryInfo&) const = default;
    };

    struct BucketCtx
    {
        int64_t id;
    };

    struct Entry
    {
        int64_t version;
        bool tombstone;
        // List element (instead of string)
        bool isList;
        KeyType type;
        std::optional<ExpiryInfo> expiry;
    };

    struct ReadRes
    {
        enum class Status
        {
            Ok,
            NotFound,
            // Key is a list
            WrongType,
            Error
        } status;
        // Value or error message
        std::string value;
        int64_t size = 0;
        // Large value (with ReadMode::Bulk), read while sending the reply
        std::shared_ptr<ObjectReader> reader;
        std::optional<ExpiryInfo> expiry;
    };

    enum class ReadMode
    {
        Value,
        // Only the size
        Size,
        // Value as RESP bulk string (avoids copying large values)
        Bulk
    };

    enum class PopStatus
    {
        Popped,
        Empty,
        // Key is no list
        WrongType,
        Error
    };

    class PreparedValue;
    class KeyLock;
    class SharedKeyLock;
    class FieldLocks;
    class CollGroupLock;
    class MultiKeyLock;
    struct Collection;
    class CollOps;
    class SmallCollOps;
    class LargeCollOps;
    struct CollState;
    struct CollLocks;

    static constexpr size_t numKeyLocks = 1024;

    std::optional<std::string> getBucketCtx(BucketCtx& ctx);
    static std::optional<std::string> checkKey(const std::string_view key);

    std::shared_mutex& keyLock(const std::string& key);
    size_t keyLockIdx(const std::string& key) const;
    // Increments the modification epoch of a key lock (for WATCH)
    void touchKeyLock(const size_t idx);

    // Versions of key, newest first. Stops after maxElements entries which are no delete markers
    bool listEntries(const int64_t bucketId, const std::string& key, std::vector<Entry>& entries,
                     const size_t maxElements = std::numeric_limits<size_t>::max());
    // Newest or oldest version of key. err is set on error
    std::optional<Entry> findEnd(const int64_t bucketId, const std::string& key, const bool newest, int& err);
    // Newest or oldest version of key, which is not a delete marker
    std::optional<Entry> findElement(const int64_t bucketId, const std::string& key, const bool newest, int& err);
    ReadRes readValue(const std::string& key, const int64_t version, const ReadMode mode);
    // Reads newest version (string value) with one lookup
    ReadRes getString(const std::string& key, const ReadMode mode);
    ReadRes readFromReader(std::shared_ptr<ObjectReader> reader, const std::string& key, const ReadMode mode);
    // Writes value to the data file (unless it was streamed already) and sets the object metadata.
    // redisData: Inline data of small collections (ObjMetadata::redisData)
    int prepareValue(const int64_t bucketId, const std::string& key, const ValueArg& value, const ContentType& contentType,
                     const std::optional<ExpiryInfo>& expiry, PreparedValue& prepared, const std::string* redisData = nullptr);
    int deleteVersion(const int64_t bucketId, const std::string& key, const int64_t version, const bool requireExisting, bool& deleted);
    // Writes value as unversioned object (version 0) and deletes all other versions. Does not commit
    int setValue(const int64_t bucketId, const std::string& key, const ValueArg& value, const std::optional<ExpiryInfo>& expiry);
    // Strings replace large collections (their entries are deleted)
    int setValue(const int64_t bucketId, const std::string& key, const ValueArg& value, const ContentType& contentType,
                 const std::optional<ExpiryInfo>& expiry, const std::string* redisData = nullptr);
    // Deletes all versions of key. existed is set if it had a value. Does not commit
    int deleteKey(const int64_t bucketId, const std::string& key, bool& existed);
    // value is read with ReadMode::Bulk
    PopStatus popElement(const int64_t bucketId, const std::string& key, const bool left, ReadRes& value, std::string& err);
    // Number of list elements of key (counted by the storage engine)
    int64_t listLength(const int64_t bucketId, const std::string& key, int& err);
    // Commits (deferred in a transaction/script)
    bool commit();
    // Commits pending modifications of the current transaction/script, so listings see them
    void syncListing();

    // Iterates over keys starting with prefix in key order, starting after startAfter. f(key, type) returns false to stop.
    // atEnd is set if all keys were iterated. Returns false on error
    bool iterateKeys(const int64_t bucketId, const std::optional<std::string>& startAfter, const std::string& prefix,
                     const std::function<bool(const std::string&, KeyType)>& f, bool& atEnd);

    // Hashes, sets and sorted sets
    // Reads the state of the hash/set/sorted set at key. Returns an error reply on error (also if the key has another type)
    std::optional<std::string> openCollection(const std::string& key, const KeyType type, CollState& cs);
    // Locks for modifying fields of key (large collections: key shared and the fields, otherwise the key exclusively,
    // also if fields is empty) and opens the collection. Returns false with err set on error
    bool locksCollection(const std::string& key, const KeyType type, const std::vector<std::string>& fields, CollLocks& locks, CollState& cs,
                         std::optional<std::string>& err);
    // Opens the collection for reading. If multiElement is set, the read is made atomic (see collGroupLocks)
    std::optional<std::string> openCollectionForRead(const std::string& key, const KeyType type, const bool multiElement, CollLocks& locks,
                                                     CollState& cs);
    // Writes a small collection (inline, or converted to a large one if too large; deletes the key if empty). Does not commit
    int writeSmallCollection(const int64_t bucketId, const std::string& key, const Collection& coll, bool& converted);
    // Deletes the entries of a large collection
    int deleteCollectionEntries(const int64_t bucketId, const std::string& key);
    // Deletes key if it is a large collection without elements. Commits
    void deleteIfEmpty(const int64_t bucketId, const std::string& key, const KeyType type);
    std::unique_ptr<CollOps> collOps(const int64_t bucketId, const std::string& key, const KeyType type, CollState& cs);
    // Bucket has entries starting with prefix
    bool hasEntries(const int64_t bucketId, const std::string& prefix);

    // Key expiry
    // Expiry of key (of its newest version). err is set on error
    std::optional<ExpiryInfo> getExpiry(const int64_t bucketId, const std::string& key, int& err);
    // Adds an entry to the expiry index. Does not commit
    int addExpiryIndex(const int64_t bucketId, const std::string& key, const int64_t expiryMs, ExpiryInfo& expiry) EXCLUDES(expiryFlagMutex);
    int removeExpiryIndex(const int64_t bucketId, const ExpiryInfo& expiry);
    // Expiry of a list (stored in its counter)
    std::optional<ExpiryInfo> listExpiry(const int64_t bucketId, const std::string& key, int& err);
    // Expiry of key with newest version entry
    std::optional<ExpiryInfo> entryExpiry(const int64_t bucketId, const std::string& key, const Entry& entry, int& err);
    // Sets the expiry of key with newest version entry (in its metadata or, for lists, in the list counter)
    int modifyExpiry(const int64_t bucketId, const std::string& key, const Entry& entry, const std::optional<ExpiryInfo>& expiry);
    // Sets the expiry time of (existing) key. Does not commit
    int setExpiry(const int64_t bucketId, const std::string& key, const int64_t expiryMs);
    // Removes the expiry of key. removed is set if it had one. Does not commit
    int clearExpiry(const int64_t bucketId, const std::string& key, bool* removed = nullptr);
    // Removes the expiry index entry of the current value of key (before it is overwritten). Returns the expiry.
    // Does not commit
    std::optional<ExpiryInfo> removeCurrentExpiry(const int64_t bucketId, const std::string& key, int& err);
    // Deletes key if it is expired. Locks the key. Commits. Returns true if it was deleted
    bool expireIfNeeded(const int64_t bucketId, const std::string& key);
    // Deletes expired keys (found via the index)
    void expireKeys() EXCLUDES(expiryFlagMutex);
    void expiryThreadFunc() EXCLUDES(expiryThreadMutex);
    // Deletes the expiry index and the entries of large collections (FLUSHDB). Returns an error reply on error
    std::optional<std::string> flushReserved(const int64_t bucketId);
    // Deletes all entries (all versions) in bucket bucketId with keys starting with prefix (and filter(version)),
    // committing in batches. Returns an error reply on error
    std::optional<std::string> deleteEntries(const int64_t bucketId, const std::string& prefix, const std::function<bool(int64_t)>& filter);

    // Key was modified, e.g. deleted because it expired (wakes up blocked commands)
    void notifyKey(const std::string& key);
    std::function<void(const std::string&)> keyChangedCallback;

    SingleFileStorage& sfs;
    const std::string bucketName;

    // Serializes modifications of a key (striped by key hash). Commands reading/modifying multiple keys atomically
    // lock all their keys (shared for reading)
    std::array<std::shared_mutex, numKeyLocks> keyLocks;
    // Locks of fields of large collections (with the key lock held shared)
    std::array<std::mutex, numKeyLocks> fieldLocks;
    // Per key lock: Field modifications of large collections (group 0) exclude reads of multiple elements (group 1),
    // so the latter are atomic (with the key lock held shared)
    std::array<GroupMutex, numKeyLocks> collGroupLocks;
    // Large collections may exist (strings replacing a collection have to delete its entries)
    std::atomic<bool> anyLargeCollections{ true };
    // Modification epoch per key lock (for WATCH)
    std::array<std::atomic<uint64_t>, numKeyLocks> keyEpochs{};

    // There may be keys with expiry (if not, expiry checks are skipped)
    std::atomic<bool> anyExpiries{ true };
    std::mutex expiryFlagMutex;
    // Incremented when an expiry is set
    uint64_t expiryGeneration GUARDED_BY(expiryFlagMutex) = 0;
    std::thread expiryThread;
    std::mutex expiryThreadMutex;
    std::condition_variable expiryThreadCond;
    bool expiryThreadStop GUARDED_BY(expiryThreadMutex) = false;

    // Held shared while executing commands, exclusively by scripts accessing undeclared keys (and while no other
    // command may run)
    folly::SharedMutex globalLock;
};
