/**
 * Copyright Martin Raiber. All Rights Reserved.
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */
#pragma once

#include "KeyStore.h"
#include "ThreadSafety.h"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <folly/executors/IOThreadPoolExecutor.h>
#include <folly/io/async/AsyncServerSocket.h>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

class AmqpConnection;

/**
 * Server for AMQP 0-9-1 (as used by RabbitMQ clients), storing messages in a versioned bucket (via KeyStore).
 *
 * - Queues and exchanges have numeric ids (looked up by name in memory). Each queue is a key ("q/<id>"), each message a
 *   version of it (body as object data; exchange,
 *   routing key, properties and publish time inline in the object metadata). Messages are deleted when they are
 *   acknowledged.
 * - The server keeps an index of the ready messages of each queue in memory (loaded from storage on startup) and
 *   tracks delivered (unacknowledged) messages per channel in memory. Unacknowledged messages are requeued if their
 *   channel/connection is closed and after a restart (all messages loaded on startup are flagged as redelivered).
 *   Messages added via S3 are only seen after a restart.
 * - Durable exchanges, queues and bindings are stored as "$m/e/<id>", "$m/q/<id>", "$m/b/<exchange id>/<queue id>/<routing
 *   key>" and "$m/x/<source exchange id>/<destination exchange id>/<routing key>" keys (values: vhost, name, flags,
 *   arguments).
 * - Exchange types direct, fanout, topic and headers, exchange to exchange bindings. Publisher confirms (sent after the
 *   message is committed), transactions (tx), basic.qos (per consumer and per channel), x-expires, x-message-ttl,
 *   per-message expiration and dead lettering (x-dead-letter-exchange/-routing-key, for rejected and expired messages).
 *
 * Authentication (PLAIN, AMQPLAIN) uses S3 access keys, authorization the S3 policies of the bucket.
 */
class AmqpServer
{
    friend class AmqpConnection;

public:
    explicit AmqpServer(KeyStore& store);
    ~AmqpServer();

    AmqpServer(const AmqpServer&) = delete;
    AmqpServer& operator=(const AmqpServer&) = delete;

    // Loads the stored definitions and messages, binds to ip:port and starts accepting connections
    bool start(const std::string& ip, int port, size_t ioThreads);

    // Closes all connections and waits for running tasks to finish
    void stop() EXCLUDES(connectionsMutex, tasksMutex);

    // Copy of a small message kept in memory, so it is delivered without reading it from storage. Shared by the
    // queues the message was routed to. Accounted in memoryBytes (released on destruction)
    struct MemoryMessage
    {
        MemoryMessage(std::string body, std::string meta, std::atomic<int64_t>& memoryBytes);
        ~MemoryMessage();
        MemoryMessage(const MemoryMessage&) = delete;
        MemoryMessage& operator=(const MemoryMessage&) = delete;

        const std::string body;
        // Serialized message meta (as stored in the object metadata)
        const std::string meta;
        std::atomic<int64_t>& memoryBytes;
        const int64_t accounted;
    };

    struct Msg
    {
        int64_t version;
        bool redelivered;
        uint32_t deliveryCount;
        // Expiry time (ms since epoch, x-message-ttl/expiration), 0 if none or unknown (loaded on startup)
        int64_t expiresAt = 0;
        // Persistent (delivery mode 2) message in a durable queue. Changes of other messages are committed lazily
        bool persistent = false;
        // If kept in memory (else read from storage)
        std::shared_ptr<const MemoryMessage> memory;
    };

    struct ChannelCounters
    {
        std::atomic<uint32_t> unacked{ 0 };
        // basic.qos with global (0: unlimited)
        std::atomic<uint32_t> globalPrefetch{ 0 };
        // channel.flow
        std::atomic<bool> flowActive{ true };
    };

    // Set on creation, except the atomics
    struct Consumer
    {
        std::weak_ptr<AmqpConnection> conn;
        uint16_t channel = 0;
        std::string tag;
        bool noAck = false;
        std::shared_ptr<ChannelCounters> counters;
        // basic.qos without global (0: unlimited)
        std::atomic<uint32_t> prefetch{ 0 };
        std::atomic<uint32_t> unacked{ 0 };
        std::atomic<bool> active{ true };
    };

    // Members before mutex are set on creation
    struct Queue
    {
        // Unique id, used in storage keys
        uint64_t id = 0;
        std::string vhost;
        std::string name;
        // Storage key of the messages
        std::string key;
        bool durable = false;
        bool exclusive = false;
        bool autoDelete = false;
        // Connection owning an exclusive queue
        int64_t owner = 0;
        std::string argsRaw;
        std::optional<int64_t> expiresMs;
        std::optional<int64_t> messageTtlMs;
        // x-dead-letter-exchange and x-dead-letter-routing-key
        std::optional<std::string> deadLetterExchange;
        std::optional<std::string> deadLetterRoutingKey;
        // Version of the next message (versions are per queue key, 0 is reserved)
        std::atomic<int64_t> nextVersion = 1;
        // Last time messages were delivered to consumers (ms since epoch, flow control)
        std::atomic<int64_t> deliveredMs{ 0 };

        std::mutex mutex;
        std::deque<Msg> ready GUARDED_BY(mutex);
        std::vector<std::shared_ptr<Consumer>> consumers GUARDED_BY(mutex);
        size_t nextConsumer GUARDED_BY(mutex) = 0;
        uint64_t unacked GUARDED_BY(mutex) = 0;
        std::chrono::steady_clock::time_point lastUsed GUARDED_BY(mutex);
        bool hadConsumers GUARDED_BY(mutex) = false;
        bool deleted GUARDED_BY(mutex) = false;
    };

    struct Binding
    {
        // Id (vhost + '\0' + name) of the destination queue or exchange
        std::string destId;
        bool toExchange = false;
        std::string routingKey;
        std::string argsRaw;
    };

    struct Exchange
    {
        // Unique id (0 for predeclared exchanges, which are not stored)
        uint64_t id = 0;
        std::string vhost;
        std::string name;
        std::string type;
        bool durable = false;
        bool autoDelete = false;
        bool internal = false;
        std::string argsRaw;
        // Guarded by AmqpServer::mutex
        std::vector<Binding> bindings;
    };

    // Channel level error
    struct Error
    {
        uint16_t code;
        std::string text;
    };

private:
    class Acceptor : public folly::AsyncServerSocket::AcceptCallback
    {
    public:
        Acceptor(AmqpServer& server, folly::EventBase* evb)
            : server(server), evb(evb)
        {
        }

        void connectionAccepted(folly::NetworkSocket fd, const folly::SocketAddress& clientAddr, AcceptInfo info) noexcept override;
        void acceptError(folly::exception_wrapper ew) noexcept override;

    private:
        AmqpServer& server;
        folly::EventBase* evb;
    };

    void onAccepted(folly::EventBase* evb, folly::NetworkSocket fd, const folly::SocketAddress& clientAddr) EXCLUDES(connectionsMutex);
    void removeConnection(AmqpConnection* conn) EXCLUDES(connectionsMutex);
    void taskStarted() EXCLUDES(tasksMutex);
    void taskFinished() EXCLUDES(tasksMutex);

    // Definitions (exchanges, queues, bindings)
    bool loadDefinitions() EXCLUDES(mutex);
    std::optional<Error> declareExchange(const std::string& vhost, const std::string& name, const std::string& type, bool passive, bool durable,
                                         bool autoDelete, bool internal, const std::string& argsRaw) EXCLUDES(mutex);
    std::optional<Error> deleteExchange(const std::string& vhost, const std::string& name, bool ifUnused) EXCLUDES(mutex);
    // name is set to the generated name if empty
    std::optional<Error> declareQueue(int64_t connId, const std::string& vhost, std::string& name, bool passive, bool durable, bool exclusive,
                                      bool autoDelete, const std::string& argsRaw, uint32_t& messageCount, uint32_t& consumerCount) EXCLUDES(mutex);
    std::optional<Error> deleteQueue(int64_t connId, const std::string& vhost, const std::string& name, bool ifUnused, bool ifEmpty,
                                     uint32_t& messageCount) EXCLUDES(mutex);
    std::optional<Error> bindQueue(int64_t connId, const std::string& vhost, const std::string& queue, const std::string& exchange,
                                   const std::string& routingKey, const std::string& argsRaw, bool bind) EXCLUDES(mutex);
    std::optional<Error> purgeQueue(int64_t connId, const std::string& vhost, const std::string& name, uint32_t& messageCount) EXCLUDES(mutex);
    // Queue accessible by connection connId
    std::shared_ptr<Queue> findQueue(int64_t connId, const std::string& vhost, const std::string& name, std::optional<Error>& err) EXCLUDES(mutex);
    // Exchange to exchange binding (exchange.bind/unbind)
    std::optional<Error> bindExchange(const std::string& vhost, const std::string& destination, const std::string& source,
                                      const std::string& routingKey, const std::string& argsRaw, bool bind) EXCLUDES(mutex);
    // Queues a message published to exchange with routingKey (and properties props) is routed to (also via exchange to
    // exchange bindings). fromClient: Published by a client (not to internal exchanges)
    std::optional<Error> route(const std::string& vhost, const std::string& exchange, const std::string& routingKey, const std::string& props,
                               bool fromClient, std::vector<std::shared_ptr<Queue>>& queues) EXCLUDES(mutex);
    // Republishes a message (body read from reader) to the dead letter exchange of queue (if it has one). Large bodies
    // are copied in chunks. reason: rejected or expired. Requests a commit if it stores a persistent message
    void deadLetter(const std::shared_ptr<Queue>& queue, const std::string& exchange, const std::string& routingKey, const std::string& props,
                    ObjectReader& reader, const std::string& reason);
    // Deletes a queue and its messages (queue lock not held)
    void removeQueue(const std::shared_ptr<Queue>& queue) EXCLUDES(mutex);
    // Deletes the exclusive queues of a connection
    void connectionClosed(int64_t connId) EXCLUDES(mutex);
    bool storeDefinition(const std::string& key, const std::string& value);
    bool deleteDefinition(const std::string& key);

    // Messages
    // Stores a message in queue (does not commit) and adds it to the ready messages. expirationMs: expiration
    // property of the message. persistent: delivery mode 2 (the message is persistent if the queue is durable as well).
    // memory: in-memory copy shared by the target queues of a message (created by the first call if the message is
    // small and the memory limit allows it)
    // backlog: set to the number of ready messages of queue afterwards (if not nullptr)
    int storeMessage(const std::shared_ptr<Queue>& queue, const ValueArg& value, const std::string& meta, std::optional<int64_t> expirationMs,
                     bool persistent, std::shared_ptr<const MemoryMessage>& memory, size_t* backlog = nullptr);
    // Removes expired messages from the head of queues (dead lettering them)
    void expireMessages() EXCLUDES(mutex);
    // Delivers ready messages to consumers with capacity
    void dispatch(const std::shared_ptr<Queue>& queue);
    // Deletes an acknowledged message
    void deleteMessage(const std::shared_ptr<Queue>& queue, const Msg& msg);
    // Returns a delivered message to the queue
    void requeue(const std::shared_ptr<Queue>& queue, Msg msg, bool countAsDelivery);
    // Delivered message was settled (acked/rejected), so the consumer/channel has capacity again
    // dispatchNow: dispatch queue (else the caller does it later)
    void settled(const std::shared_ptr<Queue>& queue, Consumer* consumer, ChannelCounters* counters, bool dispatchNow = true);
    void removeConsumer(const std::shared_ptr<Queue>& queue, const Consumer* consumer);
    // Commits in the background (coalesced)
    void requestCommit();
    // Commits in the background within the maintenance interval (changes of non-persistent messages, which do not need
    // to be durable soon)
    void requestLazyCommit();
    // Publishers are blocked (flow control) because the storage queue is too long (any thread)
    bool publishFlowBlocked();
    // Publishers to queue are blocked (flow control) because consumers do not keep up: more than backlog ready
    // messages while consumers receive messages (any thread)
    bool queueFlowBlocked(Queue& queue, size_t backlog);
    // Deletes queues unused for longer than x-expires and expires messages
    void maintenanceThreadFunc() EXCLUDES(maintenanceMutex);

    KeyStore& store;
    // Set by loadDefinitions() before accepting connections
    int64_t bucketId = 0;

    // Only accessed by start() and stop() (and serverSocket on evbs[0])
    std::unique_ptr<folly::IOThreadPoolExecutor> ioPool;
    std::vector<folly::EventBase*> evbs;
    std::vector<std::unique_ptr<Acceptor>> acceptors;
    std::shared_ptr<folly::AsyncServerSocket> serverSocket;

    // Bytes of the messages kept in memory (declared before the queues and connections holding them)
    std::atomic<int64_t> memoryMessageBytes{ 0 };

    std::mutex connectionsMutex;
    std::unordered_map<AmqpConnection*, std::shared_ptr<AmqpConnection>> connections GUARDED_BY(connectionsMutex);
    bool stopping GUARDED_BY(connectionsMutex) = false;
    std::atomic<int64_t> nextConnectionId{ 1 };

    std::mutex tasksMutex;
    std::condition_variable tasksCond;
    size_t runningTasks GUARDED_BY(tasksMutex) = 0;

    // Exchanges and queues by vhost + '\0' + name
    std::shared_mutex mutex;
    std::map<std::string, std::shared_ptr<Exchange>> exchanges GUARDED_BY(mutex);
    std::map<std::string, std::shared_ptr<Queue>> queues GUARDED_BY(mutex);

    // Next id of a queue/exchange (after the highest stored one)
    std::atomic<uint64_t> nextId{ 1 };

    std::atomic<bool> commitScheduled{ false };
    // Uncommitted changes of non-persistent messages (committed by the maintenance thread)
    std::atomic<bool> lazyCommitPending{ false };
    // Flow control state (publishFlowBlocked) and time of the last sample (us)
    std::atomic<bool> flowBlocked{ false };
    std::atomic<int64_t> flowSampleUs{ 0 };

    // Only accessed by start() and stop()
    std::thread maintenanceThread;
    std::mutex maintenanceMutex;
    std::condition_variable maintenanceCond;
    bool maintenanceStop GUARDED_BY(maintenanceMutex) = false;
};
