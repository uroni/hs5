/**
 * Copyright Martin Raiber. All Rights Reserved.
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */
#pragma once

#include "Action.h"
#include "KeyStore.h"
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
    explicit RedisServer(KeyStore& store);
    ~RedisServer();

    RedisServer(const RedisServer&) = delete;
    RedisServer& operator=(const RedisServer&) = delete;

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

    using KeyType = KeyStore::KeyType;
    using ExpiryInfo = KeyStore::ExpiryInfo;

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

    class ReplyBuilder;

    class LuaScript;

    using BucketCtx = KeyStore::BucketCtx;
    using Entry = KeyStore::Entry;
    using ReadRes = KeyStore::ReadRes;
    using ReadMode = KeyStore::ReadMode;
    using PopStatus = KeyStore::PopStatus;
    using PreparedValue = KeyStore::PreparedValue;
    using KeyLock = KeyStore::KeyLock;
    using SharedKeyLock = KeyStore::SharedKeyLock;
    using FieldLocks = KeyStore::FieldLocks;
    using CollGroupLock = KeyStore::CollGroupLock;
    using MultiKeyLock = KeyStore::MultiKeyLock;
    using Collection = KeyStore::Collection;
    using CollOps = KeyStore::CollOps;
    using SmallCollOps = KeyStore::SmallCollOps;
    using LargeCollOps = KeyStore::LargeCollOps;
    using CollState = KeyStore::CollState;
    using CollLocks = KeyStore::CollLocks;

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

    std::optional<std::string> checkAuth(const ConnState& state, const std::string_view cmd, const std::initializer_list<Action> actions,
                                         const std::optional<std::string_view> key);
    void registerWaiter(const std::vector<std::string>& keys, const std::shared_ptr<RedisConnection>& conn) EXCLUDES(waitersMutex);
    void unregisterWaiter(const std::vector<std::string>& keys, const RedisConnection* conn) EXCLUDES(waitersMutex);
    void notifyKey(const std::string& key) EXCLUDES(waitersMutex);

    // Pub/Sub. Returns the number of receivers
    int64_t publish(const std::string& channel, const std::string& message) EXCLUDES(pubsubMutex);
    void subscribe(const std::string& name, const bool pattern, const bool resp3, const std::shared_ptr<RedisConnection>& conn) EXCLUDES(pubsubMutex);
    void unsubscribe(const std::string& name, const bool pattern, const RedisConnection* conn) EXCLUDES(pubsubMutex);
    void unsubscribeAll(const RedisConnection* conn) EXCLUDES(pubsubMutex);

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
    std::optional<std::string> checkCollectionAuth(ConnState& state, const std::vector<std::string>& args, const std::string& cmd);
    // Writes small collections, releases locks, deletes empty large collections and commits
    ExecResult finishCollectionWrite(const int64_t bucketId, const std::string& key, const KeyType type, CollState& cs, CollOps& ops,
                                     CollLocks& locks, const bool modified, const bool removed, ExecResult reply);
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

    KeyStore& store;

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

    std::mutex waitersMutex;
    // Connections blocked on a key (BLPOP/BRPOP)
    std::unordered_map<std::string, std::vector<std::weak_ptr<RedisConnection>>> waiters GUARDED_BY(waitersMutex);

    std::mutex pubsubMutex;
    struct Subscriber
    {
        std::weak_ptr<RedisConnection> conn;
        bool resp3;
    };
    // Channel/pattern -> subscribed connections
    std::unordered_map<std::string, std::unordered_map<const RedisConnection*, Subscriber>> channelSubs GUARDED_BY(pubsubMutex);
    std::unordered_map<std::string, std::unordered_map<const RedisConnection*, Subscriber>> patternSubs GUARDED_BY(pubsubMutex);

    std::mutex scriptsMutex;
    // SHA1 (hex) -> Lua script
    std::unordered_map<std::string, std::string> scripts GUARDED_BY(scriptsMutex);
    // SHA1 of scripts accessing undeclared keys
    std::unordered_set<std::string> globalScripts GUARDED_BY(scriptsMutex);

    std::mutex cursorsMutex;
    // SCAN cursor id -> last returned key
    std::unordered_map<uint64_t, std::string> cursors GUARDED_BY(cursorsMutex);
    std::deque<uint64_t> cursorsOrder GUARDED_BY(cursorsMutex);
};
