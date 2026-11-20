/**
 * Copyright Martin Raiber. All Rights Reserved.
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */
#pragma once

#include "Action.h"
#include "SingleFileStorage.h"
#include "ThreadSafety.h"
#include <array>
#include <atomic>
#include <condition_variable>
#include <deque>
#include <folly/SharedMutex.h>
#include <folly/executors/IOThreadPoolExecutor.h>
#include <folly/io/async/AsyncServerSocket.h>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <shared_mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class SingleFileStorage;
class RedisConnection;
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

struct RedisCommand
{
    std::vector<std::string> args;
    // Large value arguments streamed to the data file (args has an empty string at their index).
    // Indexed like args. Empty if no argument was streamed
    std::vector<std::shared_ptr<StreamedValue>> streamed;

    std::shared_ptr<StreamedValue> streamedArg(const size_t idx) const
    {
        return idx < streamed.size() ? streamed[idx] : nullptr;
    }

    ValueArg valueArg(const size_t idx) const
    {
        return ValueArg{ .data = args[idx], .streamed = streamedArg(idx) };
    }
};

/**
 * Server for (a subset of) the Redis protocol (RESP2 and RESP3), storing keys as objects in a versioned bucket.
 *
 * - Strings are the newest version of an object. Writing a string (SET, INCR, ...) writes the unversioned object
 *   (version 0, the "null" version in S3) and deletes all other versions, so usually it is a single overwrite.
 * - Lists are all versions of an object (newest version is the head). LPUSH adds a version with a positive
 *   version number from the global version counter, RPUSH one with the negated counter, so it sorts after
 *   (is older than) all other versions. Pops delete the newest/oldest version.
 * - Delete markers (created via S3 DeleteObject) hide the object for string commands and are skipped by
 *   list commands.
 *
 * - Hashes, sets and sorted sets are an (unversioned) object each. Small ones are serialized into its metadata,
 *   larger ones are stored with one entry per element in the counters bucket (sorted sets additionally with an index
 *   by score). Modifications of fields of large collections only lock the key shared (and the fields).
 * - Key expiry (EXPIRE, SET EX, ...) is stored in the object metadata (ObjMetadata::LockInfo) of the value (of lists
 *   in their element counter, after the count), plus an index ordered by expiry time in the counters bucket (see
 *   buckets::getCountersBucket). Expired keys are hidden, deleted when accessed and by a background thread.
 * - MULTI/EXEC and Lua scripts (EVAL) lock the keys they access (as declared) and run atomically with respect to
 *   other Redis commands. Scripts accessing undeclared keys are run with the global lock held exclusively (like a
 *   single-threaded Redis server). WATCH detects modifications via the Redis protocol only (not via S3).
 * - Pub/Sub messages are only delivered to subscribers connected to this server.
 *
 * Authentication (AUTH username password) uses S3 access keys, authorization the S3 policies of the bucket.
 */
class RedisServer
{
    friend class RedisConnection;

public:
    RedisServer(SingleFileStorage& sfs, const std::string& bucket);
    ~RedisServer();

    RedisServer(const RedisServer&) = delete;
    RedisServer& operator=(const RedisServer&) = delete;

    // Lets the storage engine count list elements in Redis buckets (buckets::BucketInfo::redis). Has to be set up before
    // the storage engine is started (to count modifications replayed from the WAL)
    static void setupCounters(SingleFileStorage::SFSOptions& options);

    // Creates the bucket (with versioning enabled) if it does not exist yet and marks it as Redis bucket
    static bool initBucket(const std::string& bucket);

    // Binds to ip:port and starts accepting connections. Returns false on error
    bool start(const std::string& ip, int port, size_t ioThreads);

    // Closes all connections and waits for running commands to finish
    void stop() EXCLUDES(connectionsMutex, tasksMutex);

    // Part of a reply streamed from the data file: prefix, then the content of reader
    struct StreamPart
    {
        std::string prefix;
        std::shared_ptr<ObjectReader> reader;
    };

    struct ExecResult
    {
        std::string reply;
        // Reply continues with these parts (large values read while sending), followed by tail
        std::vector<StreamPart> streamParts;
        std::string tail;
        // Blocking command did not find anything. Retry until timeout (seconds, 0 for no timeout)
        bool block = false;
        double blockTimeout = 0;
        // Close the connection after sending the reply
        bool close = false;
        // Command (script) has to be executed again with the global lock held exclusively (internal)
        bool needsGlobalLock = false;
    };

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

    struct ConnState
    {
        int64_t id = 0;
        // Empty if not authenticated
        std::string accessKey;
        std::optional<std::string> name;
        // RESP3 protocol (via HELLO 3)
        bool resp3 = false;

        // MULTI was called. Commands are queued until EXEC
        bool inMulti = false;
        // A command could not be queued. EXEC fails
        bool multiError = false;
        std::vector<RedisCommand> multiQueue;
        // WATCHed keys and the modification epoch of their key lock when they were watched
        std::vector<std::pair<std::string, uint64_t>> watched;

        // Subscribed channels and patterns
        std::set<std::string> channels;
        std::set<std::string> patterns;

        bool subscribed() const
        {
            return !channels.empty() || !patterns.empty();
        }
    };

private:
    class Acceptor : public folly::AsyncServerSocket::AcceptCallback
    {
    public:
        Acceptor(RedisServer& server, folly::EventBase* evb)
            : server(server), evb(evb)
        {
        }

        void connectionAccepted(folly::NetworkSocket fd, const folly::SocketAddress& clientAddr, AcceptInfo info) noexcept override;

        void acceptError(folly::exception_wrapper ew) noexcept override;

    private:
        RedisServer& server;
        folly::EventBase* evb;
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

    class ReplyBuilder;

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
    struct Collection;
    class CollOps;
    class SmallCollOps;
    class LargeCollOps;
    class LuaScript;

    static constexpr size_t numKeyLocks = 1024;

    void onAccepted(folly::EventBase* evb, folly::NetworkSocket fd, const folly::SocketAddress& clientAddr) EXCLUDES(connectionsMutex);
    void removeConnection(RedisConnection* conn) EXCLUDES(connectionsMutex);

    // Executes a command (on a CPU thread)
    ExecResult exec(ConnState& state, const RedisCommand& cmd, const std::shared_ptr<RedisConnection>& conn);

    // Executes a GET of a small value directly (on the EventBase thread of the connection), reading the value via
    // mmap (see get_in_worker_threshold). Returns std::nullopt if the command has to be executed via exec
    std::optional<ExecResult> execInline(ConnState& state, const RedisCommand& cmd);
    // Executes a command (without MULTI queueing and pub/sub restrictions)
    ExecResult execCommand(ConnState& state, const RedisCommand& cmd, const std::string& name, const std::shared_ptr<RedisConnection>& conn);

    void taskStarted() EXCLUDES(tasksMutex);
    void taskFinished() EXCLUDES(tasksMutex);

    std::optional<std::string> getBucketCtx(BucketCtx& ctx);
    std::optional<std::string> checkAuth(const ConnState& state, const std::string_view cmd, const std::initializer_list<Action> actions,
                                         const std::optional<std::string_view> key);
    static std::optional<std::string> checkKey(const std::string_view key);

    std::shared_mutex& keyLock(const std::string& key);
    size_t keyLockIdx(const std::string& key) const;

    class MultiKeyLock;

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
    // Writes value to the data file (unless it was streamed already) and sets the object metadata
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

    // Hashes, sets and sorted sets
    struct CollState;
    struct CollLocks;
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
    std::optional<std::string> checkCollectionAuth(ConnState& state, const std::vector<std::string>& args, const std::string& cmd);
    // Writes small collections, releases locks, deletes empty large collections and commits
    ExecResult finishCollectionWrite(const int64_t bucketId, const std::string& key, const KeyType type, CollState& cs, CollOps& ops,
                                     CollLocks& locks, const bool modified, const bool removed, ExecResult reply);
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
    // value is read with ReadMode::Bulk
    PopStatus popElement(const int64_t bucketId, const std::string& key, const bool left, ReadRes& value, std::string& err);
    bool commit();
    // Commits pending modifications of the current transaction/script, so listings see them
    void syncListing();
    // Number of list elements of key (counted by the storage engine)
    int64_t listLength(const int64_t bucketId, const std::string& key, int& err);

    // Iterates over keys starting with prefix in key order, starting after startAfter. f(key, type) returns false to stop.
    // atEnd is set if all keys were iterated. Returns false on error
    template <typename F>
    bool iterateKeys(const int64_t bucketId, const std::optional<std::string>& startAfter, const std::string& prefix, F f, bool& atEnd);

    void registerWaiter(const std::vector<std::string>& keys, const std::shared_ptr<RedisConnection>& conn) EXCLUDES(waitersMutex);
    void unregisterWaiter(const std::vector<std::string>& keys, const RedisConnection* conn) EXCLUDES(waitersMutex);
    void notifyKey(const std::string& key) EXCLUDES(waitersMutex);

    // Pub/Sub. Returns the number of receivers
    int64_t publish(const std::string& channel, const std::string& message) EXCLUDES(pubsubMutex);
    void subscribe(const std::string& name, const bool pattern, const bool resp3, const std::shared_ptr<RedisConnection>& conn) EXCLUDES(pubsubMutex);
    void unsubscribe(const std::string& name, const bool pattern, const RedisConnection* conn) EXCLUDES(pubsubMutex);
    void unsubscribeAll(const RedisConnection* conn) EXCLUDES(pubsubMutex);

    // Increments the modification epoch of a key lock (for WATCH)
    void touchKeyLock(const size_t idx);

    // Runs the commands of a transaction (EXEC)
    ExecResult execMulti(ConnState& state, const std::shared_ptr<RedisConnection>& conn);
    ExecResult cmdAuth(ConnState& state, const std::vector<std::string>& args);
    ExecResult cmdHello(ConnState& state, const std::vector<std::string>& args);
    ExecResult cmdClient(ConnState& state, const std::vector<std::string>& args);
    ExecResult cmdGet(ConnState& state, const std::vector<std::string>& args);
    ExecResult cmdStrlen(ConnState& state, const std::vector<std::string>& args);
    ExecResult cmdMget(ConnState& state, const std::vector<std::string>& args);
    ExecResult cmdSet(ConnState& state, const RedisCommand& cmd);
    ExecResult cmdSetnx(ConnState& state, const RedisCommand& cmd);
    ExecResult cmdMset(ConnState& state, const RedisCommand& cmd);
    ExecResult cmdIncr(ConnState& state, const std::vector<std::string>& args, const std::string& cmd);
    ExecResult cmdDel(ConnState& state, const std::vector<std::string>& args);
    ExecResult cmdExists(ConnState& state, const std::vector<std::string>& args);
    ExecResult cmdType(ConnState& state, const std::vector<std::string>& args);
    ExecResult cmdPush(ConnState& state, const RedisCommand& cmd, const bool left, const bool onlyIfExists);
    ExecResult cmdPop(ConnState& state, const std::vector<std::string>& args, const bool left);
    ExecResult cmdBlockingPop(ConnState& state, const std::vector<std::string>& args, const bool left, const std::shared_ptr<RedisConnection>& conn);
    ExecResult cmdLlen(ConnState& state, const std::vector<std::string>& args);
    ExecResult cmdLrange(ConnState& state, const std::vector<std::string>& args);
    ExecResult cmdLindex(ConnState& state, const std::vector<std::string>& args);
    ExecResult cmdScan(ConnState& state, const std::vector<std::string>& args) EXCLUDES(cursorsMutex);
    ExecResult cmdKeys(ConnState& state, const std::vector<std::string>& args);
    ExecResult cmdDbsize(ConnState& state, const std::vector<std::string>& args);
    ExecResult cmdFlushdb(ConnState& state, const std::vector<std::string>& args);
    ExecResult cmdSetex(ConnState& state, const RedisCommand& cmd, const bool ms);
    ExecResult cmdGetdel(ConnState& state, const std::vector<std::string>& args);
    ExecResult cmdExpire(ConnState& state, const std::vector<std::string>& args, const std::string& cmd);
    ExecResult cmdTtl(ConnState& state, const std::vector<std::string>& args, const std::string& cmd);
    ExecResult cmdPersist(ConnState& state, const std::vector<std::string>& args);
    ExecResult cmdHash(ConnState& state, const std::vector<std::string>& args, const std::string& cmd);
    ExecResult cmdSetType(ConnState& state, const std::vector<std::string>& args, const std::string& cmd);
    ExecResult cmdZset(ConnState& state, const std::vector<std::string>& args, const std::string& cmd);
    ExecResult cmdPubsub(ConnState& state, const std::vector<std::string>& args, const std::string& cmd, const std::shared_ptr<RedisConnection>& conn);
    ExecResult cmdPublish(ConnState& state, const std::vector<std::string>& args);
    ExecResult cmdEval(ConnState& state, const std::vector<std::string>& args, const std::string& cmd, const std::shared_ptr<RedisConnection>& conn) EXCLUDES(scriptsMutex);
    ExecResult cmdScript(ConnState& state, const std::vector<std::string>& args) EXCLUDES(scriptsMutex);
    // Executes a Lua script with keys and args
    // Executes a Lua script with keys and args. If canRetry is set, the script is aborted on its first access to an
    // undeclared key (if it did not modify anything yet) and the result has needsGlobalLock set
    ExecResult runScript(ConnState& state, const std::string& sha, const std::string& body, const std::vector<std::string>& keys,
                         const std::vector<std::string>& argv, const std::shared_ptr<RedisConnection>& conn, const bool canRetry);
    // Script accesses undeclared keys, so it is run with the global lock held exclusively
    void markGlobalScript(const std::string& sha) EXCLUDES(scriptsMutex);
    // Command has to be executed with the global lock held exclusively (EVAL of a global script, EXEC with one)
    bool needsGlobalLock(const ConnState& state, const std::string& cmd, const std::vector<std::string>& args) EXCLUDES(scriptsMutex);
    bool isGlobalScript(const std::string& sha, const std::string* body) EXCLUDES(scriptsMutex);

    SingleFileStorage& sfs;
    const std::string bucketName;

    // Only accessed by start() and stop()
    std::unique_ptr<folly::IOThreadPoolExecutor> ioPool;
    std::vector<folly::EventBase*> evbs;
    std::vector<std::unique_ptr<Acceptor>> acceptors;

    // Role held while running on the EventBase of the server socket (evbs[0])
    ThreadRole acceptThread;

    void assertAcceptThread() const ASSERT_CAPABILITY(acceptThread);

    std::shared_ptr<folly::AsyncServerSocket> serverSocket GUARDED_BY(acceptThread);

    std::mutex connectionsMutex;
    std::unordered_map<RedisConnection*, std::shared_ptr<RedisConnection>> connections GUARDED_BY(connectionsMutex);
    bool stopping GUARDED_BY(connectionsMutex) = false;

    std::atomic<int64_t> nextConnectionId{ 1 };

    std::mutex tasksMutex;
    std::condition_variable tasksCond;
    size_t runningTasks GUARDED_BY(tasksMutex) = 0;

    // Serializes modifications of a key (striped by key hash). Commands reading/modifying multiple keys atomically
    // lock all their keys (shared for reading)
    std::array<std::shared_mutex, numKeyLocks> keyLocks;

    std::mutex waitersMutex;
    // Connections blocked on a key (BLPOP/BRPOP)
    std::unordered_map<std::string, std::vector<std::weak_ptr<RedisConnection>>> waiters GUARDED_BY(waitersMutex);

    // Locks of fields of large collections (with the key lock held shared)
    std::array<std::mutex, numKeyLocks> fieldLocks;
    // Per key lock: Field modifications of large collections (group 0) exclude reads of multiple elements (group 1),
    // so the latter are atomic (with the key lock held shared)
    std::array<GroupMutex, numKeyLocks> collGroupLocks;
    // Large collections may exist (strings replacing a collection have to delete its entries)
    std::atomic<bool> anyLargeCollections{ true };

    // Modification epoch per key lock (for WATCH)
    std::array<std::atomic<uint64_t>, numKeyLocks> keyEpochs{};

    std::mutex pubsubMutex;
    struct Subscriber
    {
        std::weak_ptr<RedisConnection> conn;
        bool resp3;
    };
    // Channel/pattern -> subscribed connections
    std::unordered_map<std::string, std::unordered_map<const RedisConnection*, Subscriber>> channelSubs GUARDED_BY(pubsubMutex);
    std::unordered_map<std::string, std::unordered_map<const RedisConnection*, Subscriber>> patternSubs GUARDED_BY(pubsubMutex);

    // There may be keys with expiry (if not, expiry checks are skipped)
    std::atomic<bool> anyExpiries{ true };
    std::mutex expiryFlagMutex;
    // Incremented when an expiry is set
    uint64_t expiryGeneration GUARDED_BY(expiryFlagMutex) = 0;
    std::thread expiryThread;
    std::mutex expiryThreadMutex;
    std::condition_variable expiryThreadCond;
    bool expiryThreadStop GUARDED_BY(expiryThreadMutex) = false;

    std::mutex scriptsMutex;
    // SHA1 (hex) -> Lua script
    std::unordered_map<std::string, std::string> scripts GUARDED_BY(scriptsMutex);
    // SHA1 of scripts accessing undeclared keys
    std::unordered_set<std::string> globalScripts GUARDED_BY(scriptsMutex);

    // Held shared while executing commands, exclusively by scripts accessing undeclared keys
    folly::SharedMutex globalLock;

    std::mutex cursorsMutex;
    // SCAN cursor id -> last returned key
    std::unordered_map<uint64_t, std::string> cursors GUARDED_BY(cursorsMutex);
    std::deque<uint64_t> cursorsOrder GUARDED_BY(cursorsMutex);
};
