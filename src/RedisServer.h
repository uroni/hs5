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
#include <folly/executors/IOThreadPoolExecutor.h>
#include <folly/io/async/AsyncServerSocket.h>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

class SingleFileStorage;
class RedisConnection;
class StreamedValue;
struct ContentType;
class ObjectReader;

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
    };

    struct ConnState
    {
        int64_t id = 0;
        // Empty if not authenticated
        std::string accessKey;
        std::optional<std::string> name;
        // RESP3 protocol (via HELLO 3)
        bool resp3 = false;
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

    enum class KeyType
    {
        None,
        String,
        List
    };

    class PreparedValue;

    static constexpr size_t numKeyLocks = 1024;

    void onAccepted(folly::EventBase* evb, folly::NetworkSocket fd, const folly::SocketAddress& clientAddr) EXCLUDES(connectionsMutex);
    void removeConnection(RedisConnection* conn) EXCLUDES(connectionsMutex);

    // Executes a command (on a CPU thread)
    ExecResult exec(ConnState& state, const RedisCommand& cmd, const std::shared_ptr<RedisConnection>& conn);

    // Executes a GET of a small value directly (on the EventBase thread of the connection), reading the value via
    // mmap (see get_in_worker_threshold). Returns std::nullopt if the command has to be executed via exec
    std::optional<ExecResult> execInline(ConnState& state, const RedisCommand& cmd);

    void taskStarted() EXCLUDES(tasksMutex);
    void taskFinished() EXCLUDES(tasksMutex);

    std::optional<std::string> getBucketCtx(BucketCtx& ctx);
    std::optional<std::string> checkAuth(const ConnState& state, const std::string_view cmd, const std::initializer_list<Action> actions,
                                         const std::optional<std::string_view> key);
    static std::optional<std::string> checkKey(const std::string_view key);

    std::shared_mutex& keyLock(const std::string& key);

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
    int prepareValue(const int64_t bucketId, const std::string& key, const ValueArg& value, const ContentType& contentType, PreparedValue& prepared);
    int deleteVersion(const int64_t bucketId, const std::string& key, const int64_t version, const bool requireExisting, bool& deleted);
    // Writes value as unversioned object (version 0) and deletes all other versions. Does not commit
    int setValue(const int64_t bucketId, const std::string& key, const ValueArg& value);
    // value is read with ReadMode::Bulk
    PopStatus popElement(const int64_t bucketId, const std::string& key, const bool left, ReadRes& value, std::string& err);
    bool commit();
    // Number of list elements of key (counted by the storage engine)
    int64_t listLength(const int64_t bucketId, const std::string& key, int& err);

    // Iterates over keys starting with prefix in key order, starting after startAfter. f(key, type) returns false to stop.
    // atEnd is set if all keys were iterated. Returns false on error
    template <typename F>
    bool iterateKeys(const int64_t bucketId, const std::optional<std::string>& startAfter, const std::string& prefix, F f, bool& atEnd);

    void registerWaiter(const std::vector<std::string>& keys, const std::shared_ptr<RedisConnection>& conn) EXCLUDES(waitersMutex);
    void unregisterWaiter(const std::vector<std::string>& keys, const RedisConnection* conn) EXCLUDES(waitersMutex);
    void notifyKey(const std::string& key) EXCLUDES(waitersMutex);

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

    std::mutex cursorsMutex;
    // SCAN cursor id -> last returned key
    std::unordered_map<uint64_t, std::string> cursors GUARDED_BY(cursorsMutex);
    std::deque<uint64_t> cursorsOrder GUARDED_BY(cursorsMutex);
};
