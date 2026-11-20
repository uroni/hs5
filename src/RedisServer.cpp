/**
 * Copyright Martin Raiber. All Rights Reserved.
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */
#include "RedisServer.h"
#include "Action.h"
#include "Auth.h"
#include "Buckets.h"
#include "ContentType.h"
#include "ObjectReader.h"
#include "PayloadHash.h"
#include "SingleFileStorage.h"
#include "ThreadSafety.h"
#include "data.h"
#include "s3handler.h"
#include <array>
#include <atomic>
#include <charconv>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstring>
#include <deque>
#include <folly/Random.h>
#include <folly/ScopeGuard.h>
#include <folly/executors/GlobalExecutor.h>
#include <folly/executors/IOThreadPoolExecutor.h>
#include <folly/executors/SerialExecutor.h>
#include <folly/executors/thread_factory/NamedThreadFactory.h>
#include <folly/io/IOBuf.h>
#include <folly/io/async/AsyncServerSocket.h>
#include <folly/io/async/AsyncSocket.h>
#include <folly/io/async/AsyncTimeout.h>
#include <folly/logging/xlog.h>
#include <gflags/gflags.h>
#include <limits>
#include <mutex>
#include <openssl/evp.h>
#include <openssl/md5.h>
#include <optional>
#include <sodium.h>
#include <unordered_map>
#include <unordered_set>
#include <vector>

DECLARE_bool(disable_md5_etag);
DECLARE_int64(get_in_worker_threshold);
DEFINE_int64(redis_max_value_size, 512 * 1024 * 1024, "Max size of values written or read via the Redis protocol (larger objects can only be accessed via S3)");

namespace
{
    constexpr int64_t maxMultiBulkLen = 1024 * 1024;
    constexpr size_t maxInlineLen = 64 * 1024;
    // Max length of a multi bulk/bulk header line
    constexpr size_t maxHeaderLen = 32;
    // Value arguments at least this large are written to the data file while they are received and values at least
    // this large are read from the data file while sending them
    constexpr int64_t streamThreshold = 64 * 1024;
    // Size of chunks written to/read from the data file when streaming values
    constexpr size_t streamChunkSize = 1024 * 1024;
    // Stop reading from the connection if this much received data is not yet written to the data file
    constexpr size_t streamBacklogHigh = 8 * 1024 * 1024;
    constexpr size_t streamBacklogLow = 4 * 1024 * 1024;
    // Don't read more of a streamed value while this much data is waiting to be written to the socket
    constexpr size_t sendBufferHigh = 4 * 1024 * 1024;
    // S3 key length limit
    constexpr size_t maxKeyLen = 1024;
    // Stop reading from the connection if this many commands are pending
    constexpr size_t maxPendingCommands = 1024;
    // Max number of pipelined commands executed in one task
    constexpr size_t maxBatchCommands = 128;
    // Blocked commands retry this often (in addition to being woken up by pushes via the Redis protocol), so
    // objects added via S3 are popped as well
    constexpr auto blockedPollInterval = std::chrono::milliseconds(1000);
    // Retries if the found list element/value was deleted concurrently (e.g. via S3)
    constexpr int maxRetries = 16;
    constexpr size_t maxScanCursors = 100000;
    constexpr size_t flushBatchSize = 10000;
    constexpr const char* redisVersion = "7.2.0";

    namespace resp
    {
        const std::string ok = "+OK\r\n";

        // RESP3 has a single null type
        const std::string& nullBulk(const RedisServer::ConnState& state)
        {
            static const std::string resp2 = "$-1\r\n";
            static const std::string resp3 = "_\r\n";
            return state.resp3 ? resp3 : resp2;
        }

        const std::string& nullArray(const RedisServer::ConnState& state)
        {
            static const std::string resp2 = "*-1\r\n";
            static const std::string resp3 = "_\r\n";
            return state.resp3 ? resp3 : resp2;
        }
        const std::string emptyArray = "*0\r\n";

        std::string simple(const std::string_view str)
        {
            return fmt::format("+{}\r\n", str);
        }

        std::string error(const std::string_view str)
        {
            return fmt::format("-{}\r\n", str);
        }

        std::string integer(const int64_t i)
        {
            return fmt::format(":{}\r\n", i);
        }

        std::string arrayHeader(const size_t n)
        {
            return fmt::format("*{}\r\n", n);
        }

        void appendBulk(std::string& out, const std::string_view str)
        {
            out += fmt::format("${}\r\n", str.size());
            out.append(str);
            out += "\r\n";
        }

        std::string bulk(const std::string_view str)
        {
            std::string ret;
            appendBulk(ret, str);
            return ret;
        }

        std::string wrongArgs(const std::string_view cmd)
        {
            return error(fmt::format("ERR wrong number of arguments for '{}' command", cmd));
        }

        const std::string notInteger = error("ERR value is not an integer or out of range");
        const std::string syntaxError = error("ERR syntax error");
        const std::string wrongType = error("WRONGTYPE Operation against a key holding the wrong kind of value");
    } // namespace resp

    std::string toUpper(const std::string_view str)
    {
        std::string ret(str);
        for (auto& ch : ret)
            ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
        return ret;
    }

    std::string toLower(const std::string_view str)
    {
        std::string ret(str);
        for (auto& ch : ret)
            ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        return ret;
    }

    // Parses an integer like Redis (no leading zeros, no '+', no whitespace)
    bool parseInt64(const std::string_view str, int64_t& val)
    {
        if (str.empty() || str.size() > 20)
            return false;

        const auto res = std::from_chars(str.data(), str.data() + str.size(), val);
        if (res.ec != std::errc() || res.ptr != str.data() + str.size())
            return false;

        return std::to_string(val) == str;
    }

    bool parseTimeout(const std::string& str, double& timeout)
    {
        if (str.empty())
            return false;

        char* end = nullptr;
        timeout = std::strtod(str.c_str(), &end);
        return end == str.c_str() + str.size() && std::isfinite(timeout);
    }

    // Glob-style pattern matching like Redis (*, ?, [abc], [^a-z], \x)
    bool globMatch(std::string_view pat, std::string_view str, const int nesting = 0)
    {
        if (nesting > 1000)
            return false;

        while (!pat.empty())
        {
            switch (pat[0])
            {
                case '*':
                    while (pat.size() > 1 && pat[1] == '*')
                        pat.remove_prefix(1);
                    if (pat.size() == 1)
                        return true;
                    for (size_t i = 0; i <= str.size(); ++i)
                    {
                        if (globMatch(pat.substr(1), str.substr(i), nesting + 1))
                            return true;
                    }
                    return false;
                case '?':
                    if (str.empty())
                        return false;
                    str.remove_prefix(1);
                    pat.remove_prefix(1);
                    break;
                case '[':
                {
                    pat.remove_prefix(1);
                    const bool negate = !pat.empty() && pat[0] == '^';
                    if (negate)
                        pat.remove_prefix(1);
                    bool match = false;
                    while (!pat.empty() && pat[0] != ']')
                    {
                        if (pat[0] == '\\' && pat.size() >= 2)
                        {
                            if (!str.empty() && pat[1] == str[0])
                                match = true;
                            pat.remove_prefix(2);
                        }
                        else if (pat.size() >= 3 && pat[1] == '-' && pat[2] != ']')
                        {
                            auto start = static_cast<unsigned char>(pat[0]);
                            auto end = static_cast<unsigned char>(pat[2]);
                            if (start > end)
                                std::swap(start, end);
                            if (!str.empty() && static_cast<unsigned char>(str[0]) >= start && static_cast<unsigned char>(str[0]) <= end)
                                match = true;
                            pat.remove_prefix(3);
                        }
                        else
                        {
                            if (!str.empty() && pat[0] == str[0])
                                match = true;
                            pat.remove_prefix(1);
                        }
                    }
                    if (negate)
                        match = !match;
                    if (!match || str.empty())
                        return false;
                    str.remove_prefix(1);
                    if (!pat.empty())
                        pat.remove_prefix(1);
                    break;
                }
                case '\\':
                    if (pat.size() >= 2)
                        pat.remove_prefix(1);
                    [[fallthrough]];
                default:
                    if (str.empty() || pat[0] != str[0])
                        return false;
                    str.remove_prefix(1);
                    pat.remove_prefix(1);
                    break;
            }
        }
        return str.empty();
    }

    // Literal prefix all keys matching the glob pattern have
    std::string globPrefix(const std::string_view pat)
    {
        const auto pos = pat.find_first_of("*?[\\");
        return std::string(pat.substr(0, pos));
    }

    bool isTombstone(const std::string_view md5sum)
    {
        return md5sum.size() == 1 && md5sum[0] == metadata_tombstone;
    }

    // Redis types are stored as content type. List elements have this content type, all other objects are strings
    const ContentType& listContentType()
    {
        static const ContentType contentType = contentTypeFromStr("application/cdmi-queue");
        assert(contentType.contentType != 0);
        return contentType;
    }

    // Content type of strings written via the Redis protocol
    const ContentType& stringContentType()
    {
        static const ContentType contentType = contentTypeFromStr("text/plain");
        assert(contentType.contentType != 0);
        return contentType;
    }

    bool isListElement(const std::string& md5sum)
    {
        return !isTombstone(md5sum) && S3Handler::contentTypeIdFromMetadata(md5sum) == listContentType().contentType;
    }

    // Object read by reader is a list element
    bool isListReader(ObjectReader& reader)
    {
        const auto objMetadata = reader.GetObjMetadata();
        return objMetadata != nullptr && objMetadata->contentType.contentType == listContentType().contentType;
    }

    // Writes data at offset of an object to its extents
    int writeToExtents(SingleFileStorage& sfs, const std::vector<SingleFileStorage::Ext>& extents, const size_t offset,
                       const char* data, const size_t len)
    {
        size_t pos = 0;
        while (pos < len)
        {
            const auto currOffset = static_cast<int64_t>(offset + pos);
            auto it = std::upper_bound(extents.begin(), extents.end(), SingleFileStorage::Ext(currOffset, 0, 0));
            if (it == extents.begin())
                return EIO;
            --it;
            if (it->obj_offset > currOffset || it->obj_offset + it->len <= currOffset)
                return EIO;

            const auto extOffset = currOffset - it->obj_offset;
            const SingleFileStorage::Ext currExt(currOffset, it->data_file_offset + extOffset, it->len - extOffset);
            const auto wlen = std::min(static_cast<int64_t>(len - pos), currExt.len);
            const bool completeObj = extents.size() == 1 && extents[0].len == wlen;
            const auto rc = sfs.write_ext(currExt, data + pos, static_cast<size_t>(wlen), completeObj);
            if (rc != 0)
                return rc;
            pos += static_cast<size_t>(wlen);
        }
        return 0;
    }

    enum class ParseResult
    {
        Ok,
        Incomplete,
        Error
    };

    // Position of the "\r\n" ending the header line starting at pos. Returns std::string::npos if incomplete.
    // Sets err if the line is too long or not terminated by "\r\n"
    size_t findHeaderEnd(const char* data, const size_t len, const size_t pos, std::string& err)
    {
        const size_t searchLen = std::min(len - pos, maxHeaderLen);
        const auto nl = static_cast<const char*>(std::memchr(data + pos, '\n', searchLen));
        if (nl == nullptr)
        {
            if (searchLen == maxHeaderLen)
                err = "too big header line";
            return std::string::npos;
        }
        const auto end = static_cast<size_t>(nl - data);
        if (end == pos || data[end - 1] != '\r')
        {
            err = "expected '\\r\\n'";
            return std::string::npos;
        }
        return end - 1;
    }

    // Parses an inline command (not starting with '*'). consumed is set to the length of the parsed
    // command (also if args is empty, i.e. for an empty line)
    ParseResult parseInline(const char* data, const size_t len, size_t& consumed, std::vector<std::string>& args, std::string& err)
    {
        args.clear();

        const auto nl = static_cast<const char*>(std::memchr(data, '\n', std::min(len, maxInlineLen)));
        if (nl == nullptr)
        {
            if (len >= maxInlineLen)
            {
                err = "too big inline request";
                return ParseResult::Error;
            }
            return ParseResult::Incomplete;
        }

        consumed = static_cast<size_t>(nl - data) + 1;
        std::string_view line(data, static_cast<size_t>(nl - data));
        if (!line.empty() && line.back() == '\r')
            line.remove_suffix(1);

        size_t pos = 0;
        while (pos < line.size())
        {
            const auto start = line.find_first_not_of(" \t", pos);
            if (start == std::string_view::npos)
                break;
            auto end = line.find_first_of(" \t", start);
            if (end == std::string_view::npos)
                end = line.size();
            args.emplace_back(line.substr(start, end - start));
            pos = end;
        }
        return ParseResult::Ok;
    }

    // Whether argument idx of command cmdName (upper case) is a value which can be streamed to the data file
    bool isStreamableArg(const std::string_view cmdName, const size_t idx)
    {
        if (cmdName == "SET" || cmdName == "SETNX")
            return idx == 2;
        if (cmdName == "MSET")
            return idx >= 2 && idx % 2 == 0;
        if (cmdName == "LPUSH" || cmdName == "RPUSH" || cmdName == "LPUSHX" || cmdName == "RPUSHX")
            return idx >= 2;
        return false;
    }
} // namespace

// Large value argument written to the data file while the command is received (instead of being buffered in memory).
// prepare(), write(), finish() and the command using the value run one after another on the serial executor of the
// connection (writer role). The EventBase thread of the connection collects received data in buffer.
class StreamedValue
{
public:
    // key is the key of the object the value will be written to
    StreamedValue(SingleFileStorage& sfs, const size_t size, std::string key)
        : size(size), sfs(sfs), key(std::move(key))
    {
    }

    ~StreamedValue() NO_THREAD_SAFETY_ANALYSIS
    {
        // Value was not used (e.g. command failed)
        if (!extents.empty())
            sfs.free_extents(extents);
    }

    StreamedValue(const StreamedValue&) = delete;
    StreamedValue& operator=(const StreamedValue&) = delete;

    ThreadRole writer;

    // Called by tasks on the serial executor of the connection
    void assertWriter() const ASSERT_CAPABILITY(writer)
    {
    }

    void prepare(const std::string& bucketName) REQUIRES(writer)
    {
        if (!FLAGS_disable_md5_etag && !md5Ctx.init(EVP_md5()))
        {
            err = EIO;
            return;
        }

        const auto bucketId = buckets::getBucket(bucketName);
        if (!bucketId)
        {
            err = ENOENT;
            return;
        }

        // Version is allocated when the value is written as object. Use the version with the longest encoding
        auto res = sfs.write_prepare(make_key(key, *bucketId, std::numeric_limits<int64_t>::min()), size);
        if (res.err != 0)
        {
            XLOGF(WARN, "Preparing write of streamed Redis value of size {} failed: {}", size, res.err);
            err = res.err;
            return;
        }
        extents = std::move(res.extents);
    }

    void write(const size_t offset, const std::string& data) REQUIRES(writer)
    {
        if (err != 0)
            return;

        if (md5Ctx.ctx != nullptr)
            EVP_DigestUpdate(md5Ctx.ctx, data.data(), data.size());

        const auto rc = writeToExtents(sfs, extents, offset, data.data(), data.size());
        if (rc != 0)
        {
            XLOGF(WARN, "Writing streamed Redis value failed: {}", rc);
            err = rc;
        }
    }

    void finish() REQUIRES(writer)
    {
        if (err != 0)
            return;

        if (md5Ctx.ctx != nullptr)
        {
            unsigned char md5[EVP_MAX_MD_SIZE];
            unsigned int md5Len = 0;
            if (EVP_DigestFinal_ex(md5Ctx.ctx, md5, &md5Len) != 1 || md5Len != MD5_DIGEST_LENGTH)
            {
                err = EIO;
                return;
            }
            md5sum.assign(reinterpret_cast<const char*>(md5), md5Len);
        }
        finished = true;
    }

    const size_t size;

    // Received data not yet handed to the writer, and its offset in the value. Only accessed by the EventBase
    // thread of the connection
    std::string buffer;
    size_t bufferOffset = 0;

    int err GUARDED_BY(writer) = 0;
    bool finished GUARDED_BY(writer) = false;
    // Moved out when the value is written as object
    std::vector<SingleFileStorage::Ext> extents GUARDED_BY(writer);
    // Binary MD5 of the value (empty with disable_md5_etag)
    std::string md5sum GUARDED_BY(writer);

private:
    SingleFileStorage& sfs;
    const std::string key;
    EvpMdCtx md5Ctx GUARDED_BY(writer);
};

// Value written to the data file, but not added as object yet. Frees the data if it is not added
class RedisServer::PreparedValue
{
public:
    explicit PreparedValue(SingleFileStorage& sfs)
        : sfs(sfs)
    {
    }

    ~PreparedValue()
    {
        if (!extents.empty())
            sfs.free_extents(extents);
    }

    PreparedValue(const PreparedValue&) = delete;
    PreparedValue& operator=(const PreparedValue&) = delete;

    // Adds the value as object fn. If matchInfo does not match, the value is not added and can be added again
    int add(const std::string& fn, SingleFileStorage::MatchInfo* matchInfo)
    {
        const auto lastModified = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        const auto rc = sfs.write_finalize(fn, extents, lastModified, metadata, false, true, nullptr, matchInfo);
        if (rc == 0 && matchInfo && !matchInfo->match)
            return 0;
        extents.clear();
        return rc;
    }

    std::vector<SingleFileStorage::Ext> extents;
    std::string metadata;

private:
    SingleFileStorage& sfs;
};

// Builds a reply. Large values are streamed from the data file instead of copying them into the reply
class RedisServer::ReplyBuilder
{
public:
    void append(const std::string_view str)
    {
        curr.append(str);
    }

    // Appends a value read with ReadMode::Bulk
    void appendBulk(ReadRes& res)
    {
        if (res.reader)
        {
            curr += fmt::format("${}\r\n", res.size);
            parts.push_back(StreamPart{ .prefix = std::move(curr), .reader = std::move(res.reader) });
            curr = "\r\n";
        }
        else
        {
            curr.append(res.value);
        }
    }

    ExecResult finish()
    {
        ExecResult ret;
        if (parts.empty())
        {
            ret.reply = std::move(curr);
        }
        else
        {
            ret.streamParts = std::move(parts);
            ret.tail = std::move(curr);
        }
        return ret;
    }

private:
    std::string curr;
    std::vector<StreamPart> parts;
};

// Connection of a Redis client. Lives on one EventBase (evb). Commands are executed one after another on the serial
// executor of the connection (on CPU threads). The command currently executed and the connection state (currentCmd,
// state) are owned by the task while a command is in flight, otherwise by the EventBase thread (cmdOwner).
// Large value arguments are written to the data file while they are received (StreamedValue), large values in replies
// are read from the data file while sending
class RedisConnection : public folly::AsyncTransport::ReadCallback,
                        public folly::AsyncTransport::WriteCallback,
                        public std::enable_shared_from_this<RedisConnection>
{
public:
    RedisConnection(RedisServer& server, folly::EventBase* evb, folly::AsyncSocket::UniquePtr sock, const int64_t id)
        : server(server), evb(evb), serial(folly::SerialExecutor::create()), sock(std::move(sock))
    {
        cmdState.id = id;
    }

    // Role held while running on evb
    ThreadRole evbThread;

    void assertOnEvb() const ASSERT_CAPABILITY(evbThread)
    {
        assert(evb->isInEventBaseThread());
    }

    // Call on evb
    void start() REQUIRES(evbThread)
    {
        blockTimer = folly::AsyncTimeout::make(*evb, [this]() noexcept
                                               {
                                                   assertOnEvb();
                                                   onBlockTimer(); });
        sock->setReadCB(this);
    }

    // Call on evb. Closes the connection immediately
    void close() REQUIRES(evbThread)
    {
        if (closed)
            return;

        auto self = shared_from_this();

        closed = true;
        if (blocked && !inFlight)
        {
            assertCmdOwnerOnEvb();
            server.unregisterWaiter(blockedKeys(), this);
        }
        blocked = false;
        blockTimer.reset();
        sendState.reset();
        parseState = ParseState();
        pending.clear();
        replies.clear();
        if (sock)
        {
            sock->setReadCB(nullptr);
            sock->closeNow();
            sock.reset();
        }
        server.removeConnection(this);
    }

    // Wakes up a blocked command (any thread)
    static void wake(std::shared_ptr<RedisConnection> conn)
    {
        auto evb = conn->evb;
        evb->runInEventBaseThread([conn = std::move(conn)]()
                                  {
            conn->assertOnEvb();
            if (conn->blocked && !conn->inFlight && !conn->closed)
            {
                conn->assertCmdOwnerOnEvb();
                conn->retryBlocked();
            } });
    }

    void getReadBuffer(void** bufReturn, size_t* lenReturn) noexcept override
    {
        assertOnEvb();
        constexpr size_t minFree = 16 * 1024;
        if (inbuf.size() - inLen < minFree)
            inbuf.resize(std::max(inLen + 64 * 1024, inbuf.size() + inbuf.size() / 2));
        *bufReturn = inbuf.data() + inLen;
        *lenReturn = inbuf.size() - inLen;
    }

    void readDataAvailable(size_t len) noexcept override
    {
        assertOnEvb();
        inLen += len;
        parseInput();
    }

    void readEOF() noexcept override
    {
        assertOnEvb();
        close();
    }

    void readErr(const folly::AsyncSocketException& ex) noexcept override
    {
        assertOnEvb();
        XLOGF(DBG0, "Redis connection {} read error: {}", connId(), ex.what());
        close();
    }

    // Write of a streamed reply chunk finished
    void writeSuccess() noexcept override
    {
        assertOnEvb();
        assert(!streamWrites.empty());
        streamWriteBytes -= streamWrites.front();
        streamWrites.pop_front();
        continueSending();
    }

    void writeErr(size_t /*bytesWritten*/, const folly::AsyncSocketException& ex) noexcept override
    {
        assertOnEvb();
        XLOGF(DBG0, "Redis connection {} write error: {}", connId(), ex.what());
        if (!streamWrites.empty())
        {
            streamWriteBytes -= streamWrites.front();
            streamWrites.pop_front();
        }
        close();
    }

    folly::EventBase* getEventBase() const
    {
        return evb;
    }

private:
    // Owner of currentCmd and cmdState. See class comment
    ThreadRole cmdOwner;

    // On evb, the EventBase thread owns the command if none is in flight
    void assertCmdOwnerOnEvb() const REQUIRES(evbThread) ASSERT_CAPABILITY(cmdOwner)
    {
        assert(!inFlight);
    }

    // The task executing the command owns it
    void assertCmdOwnerInTask() const ASSERT_CAPABILITY(cmdOwner)
    {
        assert(!evb->isInEventBaseThread());
    }

    int64_t connId() const NO_THREAD_SAFETY_ANALYSIS
    {
        // Set on construction only
        return cmdState.id;
    }

    void protocolErrorOccurred(const std::string& err) REQUIRES(evbThread)
    {
        XLOGF(INFO, "Redis connection {} protocol error: {}", connId(), err);
        protocolError = err;
        // Reply after all pending commands
        processPending();
    }

    // Parses received data. Multi bulk commands are parsed incrementally, so (large) bulks do not have to be in inbuf
    // completely
    void parseInput() REQUIRES(evbThread)
    {
        while (!closed && !protocolError)
        {
            const char* data = inbuf.data() + inPos;
            const size_t avail = inLen - inPos;
            auto& ps = parseState;

            if (!ps.inMultiBulk)
            {
                if (avail == 0)
                    break;

                std::string err;
                if (data[0] != '*')
                {
                    size_t consumed = 0;
                    std::vector<std::string> args;
                    const auto res = parseInline(data, avail, consumed, args, err);
                    if (res == ParseResult::Incomplete)
                        break;
                    if (res == ParseResult::Error)
                    {
                        protocolErrorOccurred(err);
                        return;
                    }
                    inPos += consumed;
                    if (!args.empty())
                        pending.push_back(RedisCommand{ .args = std::move(args) });
                    continue;
                }

                const auto headerEnd = findHeaderEnd(data, avail, 1, err);
                if (headerEnd == std::string::npos)
                {
                    if (!err.empty())
                    {
                        protocolErrorOccurred(err);
                        return;
                    }
                    break;
                }

                int64_t count;
                if (!parseInt64(std::string_view(data + 1, headerEnd - 1), count) || count > maxMultiBulkLen)
                {
                    protocolErrorOccurred("invalid multibulk length");
                    return;
                }

                inPos += headerEnd + 2;
                if (count > 0)
                {
                    ps.inMultiBulk = true;
                    ps.remainingArgs = count;
                    ps.cmd = RedisCommand();
                    ps.cmd.args.reserve(static_cast<size_t>(std::min(count, static_cast<int64_t>(1024))));
                }
                continue;
            }

            if (ps.bulkLen < 0)
            {
                // Bulk header
                if (avail == 0)
                    break;

                if (data[0] != '$')
                {
                    protocolErrorOccurred(fmt::format("expected '$', got '{}'", data[0]));
                    return;
                }

                std::string err;
                const auto headerEnd = findHeaderEnd(data, avail, 1, err);
                if (headerEnd == std::string::npos)
                {
                    if (!err.empty())
                    {
                        protocolErrorOccurred(err);
                        return;
                    }
                    break;
                }

                int64_t bulkLen;
                if (!parseInt64(std::string_view(data + 1, headerEnd - 1), bulkLen) || bulkLen < 0 ||
                    bulkLen > FLAGS_redis_max_value_size)
                {
                    protocolErrorOccurred("invalid bulk length");
                    return;
                }

                inPos += headerEnd + 2;
                ps.bulkLen = bulkLen;
                ps.bulkFilled = 0;

                const auto argIdx = ps.cmd.args.size();
                ps.cmd.args.emplace_back();
                if (bulkLen >= streamThreshold && argIdx > 0 && isStreamableArg(ps.cmdName, argIdx))
                {
                    // Key precedes the value (MSET key value [key value ...], other commands: CMD key value ...)
                    const auto& key = ps.cmdName == "MSET" ? ps.cmd.args[argIdx - 1] : ps.cmd.args[1];
                    ps.stream = std::make_shared<StreamedValue>(server.sfs, static_cast<size_t>(bulkLen), key);
                    ps.cmd.streamed.resize(argIdx + 1);
                    ps.cmd.streamed[argIdx] = ps.stream;
                    runStreamTask([stream = ps.stream, &bucketName = server.bucketName]()
                                  {
                                      stream->assertWriter();
                                      stream->prepare(bucketName); });
                }
                else
                {
                    ps.cmd.args.back().reserve(static_cast<size_t>(std::min(bulkLen, streamThreshold)));
                }
                continue;
            }

            if (ps.bulkFilled < static_cast<size_t>(ps.bulkLen))
            {
                // Bulk data
                const auto take = std::min(avail, static_cast<size_t>(ps.bulkLen) - ps.bulkFilled);
                if (take == 0)
                    break;

                if (ps.stream)
                {
                    ps.stream->buffer.append(data, take);
                    if (ps.stream->buffer.size() >= streamChunkSize)
                        flushStream(ps.stream);
                }
                else
                {
                    ps.cmd.args.back().append(data, take);
                }
                inPos += take;
                ps.bulkFilled += take;
                continue;
            }

            // Bulk end
            if (avail < 2)
                break;

            if (data[0] != '\r' || data[1] != '\n')
            {
                protocolErrorOccurred("expected '\\r\\n' after bulk");
                return;
            }
            inPos += 2;

            if (ps.stream)
            {
                flushStream(ps.stream);
                runStreamTask([stream = ps.stream]()
                              {
                                  stream->assertWriter();
                                  stream->finish(); });
                ps.stream.reset();
            }
            else if (ps.cmd.args.size() == 1)
            {
                ps.cmdName = toUpper(ps.cmd.args[0]);
            }

            ps.bulkLen = -1;
            if (--ps.remainingArgs == 0)
            {
                pending.push_back(std::move(ps.cmd));
                ps = ParseState();
            }
        }

        if (closed)
            return;

        // Compact input buffer
        if (inPos > 0)
        {
            std::memmove(inbuf.data(), inbuf.data() + inPos, inLen - inPos);
            inLen -= inPos;
            inPos = 0;
        }

        if (inbuf.size() > 1024 * 1024 && inLen < inbuf.size() / 4)
        {
            inbuf.resize(std::max(inLen, static_cast<size_t>(64 * 1024)));
            inbuf.shrink_to_fit();
        }

        processPending();
    }

    // Runs f on the serial executor (after all previously added tasks)
    template <typename F>
    void runStreamTask(F f) REQUIRES(evbThread)
    {
        server.taskStarted();
        serial->add([self = shared_from_this(), f = std::move(f)]() mutable
                    {
                        f();
                        self->server.taskFinished(); });
    }

    // Hands the buffered data of a streamed value to the writer
    void flushStream(const std::shared_ptr<StreamedValue>& stream) REQUIRES(evbThread)
    {
        if (stream->buffer.empty())
            return;

        auto data = std::move(stream->buffer);
        stream->buffer = std::string();
        stream->buffer.reserve(streamChunkSize);
        const auto offset = stream->bufferOffset;
        stream->bufferOffset += data.size();
        const auto dataSize = data.size();
        streamBacklog += dataSize;
        updateReading();

        server.taskStarted();
        serial->add([self = shared_from_this(), stream, data = std::move(data), offset, dataSize]() mutable
                    {
                        stream->assertWriter();
                        stream->write(offset, data);
                        data = std::string();
                        const auto prevBacklog = self->streamBacklog.fetch_sub(dataSize);
                        if (prevBacklog >= streamBacklogLow && prevBacklog - dataSize < streamBacklogLow)
                        {
                            // Resume reading
                            auto evb = self->evb;
                            evb->runInEventBaseThread([self = std::move(self)]()
                                                      {
                                                          self->assertOnEvb();
                                                          self->updateReading();
                                                          self->server.taskFinished(); });
                            return;
                        }
                        self->server.taskFinished(); });
    }

    // Pauses/resumes reading from the socket depending on the number of pending commands and the amount of
    // received data not yet written to the data file
    void updateReading() REQUIRES(evbThread)
    {
        if (closed || !sock)
            return;

        const auto backlog = streamBacklog.load();
        if (!readPaused && (protocolError || pending.size() >= maxPendingCommands || backlog >= streamBacklogHigh))
        {
            sock->setReadCB(nullptr);
            readPaused = true;
        }
        else if (readPaused && !protocolError && pending.size() < maxPendingCommands / 2 && backlog < streamBacklogLow)
        {
            readPaused = false;
            sock->setReadCB(this);
        }
    }

    void processPending() REQUIRES(evbThread)
    {
        // Execute GETs of small values directly (get_in_worker_threshold). Only if no command is in flight, so the
        // order of replies stays the same
        if (FLAGS_get_in_worker_threshold > 0)
        {
            bool executed = false;
            while (!closed && !inFlight && !blocked && !pending.empty() && replies.size() < maxPendingCommands)
            {
                assertCmdOwnerOnEvb();
                auto res = server.execInline(cmdState, pending.front());
                if (!res)
                    break;
                pending.pop_front();
                replies.push_back(std::move(*res));
                executed = true;
            }

            if (executed)
            {
                sendReplies();
                if (closed)
                    return;
            }
        }

        // Execute pipelined commands in batches. Don't execute more while too many replies wait to be sent
        if (!closed && !inFlight && !blocked && !pending.empty() && replies.size() < maxPendingCommands)
        {
            assertCmdOwnerOnEvb();
            std::vector<RedisCommand> cmds;
            const auto num = std::min(pending.size(), maxBatchCommands);
            cmds.reserve(num);
            for (size_t i = 0; i < num; ++i)
            {
                cmds.push_back(std::move(pending.front()));
                pending.pop_front();
            }
            dispatch(std::move(cmds));
        }

        if (closed)
            return;

        if (protocolError && !inFlight && !blocked && pending.empty() && replies.empty() && !sendState)
        {
            write(resp::error(fmt::format("ERR Protocol error: {}", *protocolError)));
            close();
            return;
        }

        updateReading();
    }

    struct BatchResult
    {
        std::vector<RedisServer::ExecResult> results;
        // Commands not executed because a command blocked
        std::vector<RedisCommand> notExecuted;
    };

    // Executes cmds one after another in one task on the serial executor. Stops at a blocking command (which
    // stays in currentCmd) or one closing the connection. Moves cmdOwner to the task until onBatchResult
    void dispatch(std::vector<RedisCommand> cmds) REQUIRES(evbThread, cmdOwner)
    {
        batch = std::move(cmds);
        inFlight = true;
        server.taskStarted();
        serial->add([self = shared_from_this()]() mutable
                    {
            self->assertCmdOwnerInTask();
            BatchResult batchRes;
            batchRes.results.reserve(self->batch.size());
            for (size_t i = 0; i < self->batch.size(); ++i)
            {
                self->currentCmd = std::move(self->batch[i]);
                RedisServer::ExecResult res;
                try
                {
                    res = self->server.exec(self->cmdState, self->currentCmd, self);
                }
                catch (const std::exception& e)
                {
                    XLOGF(ERR, "Exception executing Redis command: {}", e.what());
                    res = RedisServer::ExecResult{ .reply = resp::error("ERR internal error") };
                }
                // Release streamed values (frees unused extents)
                self->currentCmd.streamed.clear();

                const bool block = res.block;
                const bool close = res.close;
                batchRes.results.push_back(std::move(res));
                if (block)
                {
                    batchRes.notExecuted.assign(std::make_move_iterator(self->batch.begin() + static_cast<std::ptrdiff_t>(i) + 1),
                                                std::make_move_iterator(self->batch.end()));
                    break;
                }
                if (close)
                    break;
            }
            self->batch.clear();
            auto evb = self->evb;
            evb->runInEventBaseThread([self = std::move(self), batchRes = std::move(batchRes)]() mutable
                                      {
                                          self->assertOnEvb();
                                          self->onBatchResult(std::move(batchRes)); }); });
    }

    void onBatchResult(BatchResult batchRes) REQUIRES(evbThread)
    {
        inFlight = false;
        SCOPE_EXIT { server.taskFinished(); };

        assertCmdOwnerOnEvb();

        const bool lastBlocks = !batchRes.results.empty() && batchRes.results.back().block;

        if (closed)
        {
            // Retry of blocked command registered again
            if (lastBlocks)
                server.unregisterWaiter(blockedKeys(), this);
            return;
        }

        // Execute after the blocked command
        for (auto it = batchRes.notExecuted.rbegin(); it != batchRes.notExecuted.rend(); ++it)
            pending.push_front(std::move(*it));

        double blockTimeout = 0;
        for (auto& res : batchRes.results)
        {
            if (res.block)
            {
                blockTimeout = res.blockTimeout;
                break;
            }

            if (blocked)
            {
                // Retry of blocked command succeeded
                blocked = false;
                blockTimer->cancelTimeout();
            }
            replies.push_back(std::move(res));
        }

        if (lastBlocks)
            onBlock(blockTimeout);

        sendReplies();
        if (closed)
            return;
        processPending();
    }

    void onBlock(const double blockTimeout) REQUIRES(evbThread, cmdOwner)
    {
        const auto now = std::chrono::steady_clock::now();
        if (!blocked)
        {
            blocked = true;
            blockDeadline = blockTimeout > 0 ? now + std::chrono::duration_cast<std::chrono::steady_clock::duration>(std::chrono::duration<double>(blockTimeout))
                                             : std::chrono::steady_clock::time_point::max();
        }

        if (now >= blockDeadline)
        {
            server.unregisterWaiter(blockedKeys(), this);
            blocked = false;
            replies.push_back(RedisServer::ExecResult{ .reply = resp::nullArray(cmdState) });
            return;
        }

        const auto remaining = blockDeadline - now;
        const auto interval = remaining < blockedPollInterval ? std::chrono::duration_cast<std::chrono::milliseconds>(remaining) : blockedPollInterval;
        blockTimer->scheduleTimeout(std::max(interval, std::chrono::milliseconds(1)));
    }

    // Sends queued replies in order. A reply with streamed parts is sent completely before the next one
    // Sends queued replies in order. A reply with streamed parts is sent completely before the next one. Consecutive
    // replies (e.g. of pipelined commands) are written together, with one syscall
    void sendReplies() REQUIRES(evbThread)
    {
        folly::IOBufQueue out(folly::IOBufQueue::cacheChainLength());
        const auto append = [&out](std::string data)
        {
            if (data.empty())
                return;
            // Copy small replies into one buffer, chain large ones
            if (data.size() < 4096)
                out.append(data.data(), data.size());
            else
                out.append(folly::IOBuf::fromString(std::move(data)));
        };
        const auto flush = [this, &out]() REQUIRES(evbThread)
        {
            if (!out.empty() && sock)
                sock->writeChain(nullptr, out.move());
        };

        while (!closed && !sendState && !replies.empty())
        {
            auto res = std::move(replies.front());
            replies.pop_front();

            if (!res.streamParts.empty())
            {
                sendState = std::make_unique<SendState>();
                sendState->parts = std::move(res.streamParts);
                sendState->tail = std::move(res.tail);
                sendState->closeAfter = res.close;
                append(std::move(res.reply));
                flush();
                continueSending();
                return;
            }

            append(std::move(res.reply));

            if (res.close)
            {
                flush();
                close();
                return;
            }
        }

        flush();
    }

    // Sends the streamed parts of a reply. Reads the next chunk of a value if not too much data is waiting to be
    // written to the socket
    void continueSending() REQUIRES(evbThread)
    {
        while (sendState && !closed)
        {
            auto& st = *sendState;
            if (st.partIdx >= st.parts.size())
            {
                write(std::move(st.tail));
                const auto closeAfter = st.closeAfter;
                sendState.reset();
                if (closeAfter)
                {
                    close();
                    return;
                }
                sendReplies();
                if (closed)
                    return;
                processPending();
                return;
            }

            auto& part = st.parts[st.partIdx];
            if (!st.prefixWritten)
            {
                write(std::move(part.prefix));
                st.prefixWritten = true;
            }

            const auto valueSize = part.reader->FileSize();
            if (st.offset >= valueSize)
            {
                ++st.partIdx;
                st.offset = 0;
                st.prefixWritten = false;
                continue;
            }

            if (st.readInFlight || streamWriteBytes >= sendBufferHigh)
                return;

            st.readInFlight = true;
            const auto len = std::min(static_cast<int64_t>(streamChunkSize), valueSize - st.offset);
            server.taskStarted();
            serial->add([self = shared_from_this(), reader = part.reader, offset = st.offset, len]() mutable
                        {
                            std::string chunk;
                            bool ok = true;
                            try
                            {
                                chunk.resize(static_cast<size_t>(len));
                                reader->Read(chunk.data(), len, offset);
                            }
                            catch (const ObjectReadError& e)
                            {
                                XLOGF(WARN, "Error reading streamed Redis reply: {}", e.what());
                                ok = false;
                            }
                            reader.reset();
                            auto evb = self->evb;
                            evb->runInEventBaseThread([self = std::move(self), chunk = std::move(chunk), ok]() mutable
                                                      {
                                                          self->assertOnEvb();
                                                          self->onChunkRead(std::move(chunk), ok);
                                                          self->server.taskFinished(); }); });
            return;
        }
    }

    void onChunkRead(std::string chunk, const bool ok) REQUIRES(evbThread)
    {
        if (closed || !sendState)
            return;

        sendState->readInFlight = false;

        if (!ok)
        {
            // Reply is incomplete. Client cannot recover
            close();
            return;
        }

        sendState->offset += static_cast<int64_t>(chunk.size());
        streamWrites.push_back(chunk.size());
        streamWriteBytes += chunk.size();
        sock->writeChain(this, folly::IOBuf::fromString(std::move(chunk)));
        // writeSuccess/writeErr may already have been called
        continueSending();
    }

    void onBlockTimer() REQUIRES(evbThread)
    {
        if (!blocked || closed || inFlight)
            return;

        assertCmdOwnerOnEvb();

        if (std::chrono::steady_clock::now() >= blockDeadline)
        {
            server.unregisterWaiter(blockedKeys(), this);
            blocked = false;
            replies.push_back(RedisServer::ExecResult{ .reply = resp::nullArray(cmdState) });
            sendReplies();
            if (closed)
                return;
            processPending();
            return;
        }

        retryBlocked();
    }

    // Executes the blocked command (currentCmd) again
    void retryBlocked() REQUIRES(evbThread, cmdOwner)
    {
        std::vector<RedisCommand> cmds;
        cmds.push_back(std::move(currentCmd));
        dispatch(std::move(cmds));
    }

    // Keys of the blocked command (BLPOP key [key ...] timeout)
    std::vector<std::string> blockedKeys() const REQUIRES(cmdOwner)
    {
        const auto& args = currentCmd.args;
        if (args.size() < 3)
            return {};
        return std::vector<std::string>(args.begin() + 1, args.end() - 1);
    }

    void write(std::string data) REQUIRES(evbThread)
    {
        if (!sock || data.empty())
            return;
        sock->writeChain(nullptr, folly::IOBuf::fromString(std::move(data)));
    }

    struct ParseState
    {
        bool inMultiBulk = false;
        int64_t remainingArgs = 0;
        // Length of the current bulk. -1 if the bulk header is expected next
        int64_t bulkLen = -1;
        size_t bulkFilled = 0;
        RedisCommand cmd;
        // Upper case command name (after the first argument was parsed)
        std::string cmdName;
        // Current bulk is streamed to the data file
        std::shared_ptr<StreamedValue> stream;
    };

    struct SendState
    {
        std::vector<RedisServer::StreamPart> parts;
        std::string tail;
        size_t partIdx = 0;
        bool prefixWritten = false;
        // Offset in the value of the current part
        int64_t offset = 0;
        bool readInFlight = false;
        bool closeAfter = false;
    };

    RedisServer& server;
    folly::EventBase* const evb;
    // Executes commands and streamed value reads/writes in order
    const folly::Executor::KeepAlive<folly::SerialExecutor> serial;

    RedisServer::ConnState cmdState GUARDED_BY(cmdOwner);
    // Commands executed by the current task
    std::vector<RedisCommand> batch GUARDED_BY(cmdOwner);
    // Command currently/last executed (the blocked command while blocked)
    RedisCommand currentCmd GUARDED_BY(cmdOwner);

    folly::AsyncSocket::UniquePtr sock GUARDED_BY(evbThread);

    std::string inbuf GUARDED_BY(evbThread);
    size_t inLen GUARDED_BY(evbThread) = 0;
    size_t inPos GUARDED_BY(evbThread) = 0;
    ParseState parseState GUARDED_BY(evbThread);
    std::deque<RedisCommand> pending GUARDED_BY(evbThread);
    // Results of executed commands waiting to be sent (in order)
    std::deque<RedisServer::ExecResult> replies GUARDED_BY(evbThread);
    std::optional<std::string> protocolError GUARDED_BY(evbThread);

    // Received streamed value data not yet written to the data file
    std::atomic<size_t> streamBacklog{ 0 };

    // Reply currently streamed to the client
    std::unique_ptr<SendState> sendState GUARDED_BY(evbThread);
    // Sizes of streamed reply chunks being written to the socket (in order)
    std::deque<size_t> streamWrites GUARDED_BY(evbThread);
    size_t streamWriteBytes GUARDED_BY(evbThread) = 0;

    bool closed GUARDED_BY(evbThread) = false;
    bool inFlight GUARDED_BY(evbThread) = false;
    bool readPaused GUARDED_BY(evbThread) = false;

    bool blocked GUARDED_BY(evbThread) = false;
    std::chrono::steady_clock::time_point blockDeadline GUARDED_BY(evbThread);
    std::unique_ptr<folly::AsyncTimeout> blockTimer GUARDED_BY(evbThread);
};

void RedisServer::Acceptor::connectionAccepted(folly::NetworkSocket fd, const folly::SocketAddress& clientAddr, AcceptInfo /*info*/) noexcept
{
    server.onAccepted(evb, fd, clientAddr);
}

void RedisServer::Acceptor::acceptError(folly::exception_wrapper ew) noexcept
{
    XLOGF(WARN, "Error accepting Redis connection: {}", ew.what());
}

void RedisServer::taskStarted()
{
    std::scoped_lock lock(tasksMutex);
    ++runningTasks;
}

void RedisServer::taskFinished()
{
    std::scoped_lock lock(tasksMutex);
    --runningTasks;
    if (runningTasks == 0)
        tasksCond.notify_all();
}

std::shared_mutex& RedisServer::keyLock(const std::string& key)
{
    return keyLocks[std::hash<std::string>{}(key) % keyLocks.size()];
}

// Locks the key locks of multiple keys, so multi-key commands are atomic. Locks in address order (each lock once), so
// it cannot deadlock with other multi-key commands. Single key commands only hold one lock at a time
class RedisServer::MultiKeyLock
{
public:
    MultiKeyLock(RedisServer& server, const std::vector<std::string>& args, const size_t start, const size_t step, const bool exclusive) NO_THREAD_SAFETY_ANALYSIS
        : exclusive(exclusive)
    {
        for (size_t i = start; i < args.size(); i += step)
            locks.push_back(&server.keyLock(args[i]));

        std::sort(locks.begin(), locks.end());
        locks.erase(std::unique(locks.begin(), locks.end()), locks.end());

        for (auto lock : locks)
        {
            if (exclusive)
                lock->lock();
            else
                lock->lock_shared();
        }
    }

    ~MultiKeyLock() NO_THREAD_SAFETY_ANALYSIS
    {
        for (auto it = locks.rbegin(); it != locks.rend(); ++it)
        {
            if (exclusive)
                (*it)->unlock();
            else
                (*it)->unlock_shared();
        }
    }

    MultiKeyLock(const MultiKeyLock&) = delete;
    MultiKeyLock& operator=(const MultiKeyLock&) = delete;

private:
    const bool exclusive;
    std::vector<std::shared_mutex*> locks;
};

void RedisServer::assertAcceptThread() const
{
    assert(!evbs.empty() && evbs[0]->isInEventBaseThread());
}

bool RedisServer::start(const std::string& ip, int port, size_t ioThreads)
{
    ioPool = std::make_unique<folly::IOThreadPoolExecutor>(std::max(ioThreads, static_cast<size_t>(1)), std::make_shared<folly::NamedThreadFactory>("redis io"));
    for (auto& evb : ioPool->getAllEventBases())
        evbs.push_back(evb.get());

    auto mainEvb = evbs[0];
    bool ok = false;
    mainEvb->runInEventBaseThreadAndWait([&]()
                                         {
        assertAcceptThread();
        try
        {
            serverSocket = folly::AsyncServerSocket::newSocket(mainEvb);
            serverSocket->bind(folly::SocketAddress(ip, static_cast<uint16_t>(port), true));
            serverSocket->listen(1024);
            for (auto evb : evbs)
            {
                acceptors.push_back(std::make_unique<Acceptor>(*this, evb));
                serverSocket->addAcceptCallback(acceptors.back().get(), evb);
            }
            serverSocket->startAccepting();
            ok = true;
        }
        catch (const std::exception& e)
        {
            XLOGF(ERR, "Error starting Redis server on {}:{}: {}", ip, port, e.what());
            serverSocket.reset();
        } });

    if (ok)
        XLOGF(INFO, "Redis server listening on {}:{} (bucket {})", ip, port, bucketName);

    return ok;
}

void RedisServer::stop()
{
    if (!ioPool)
        return;

    evbs[0]->runInEventBaseThreadAndWait([&]()
                                         {
        assertAcceptThread();
        if (serverSocket)
        {
            serverSocket->stopAccepting();
            serverSocket.reset();
        } });

    std::vector<std::shared_ptr<RedisConnection>> conns;
    {
        std::scoped_lock lock(connectionsMutex);
        stopping = true;
        for (auto& [ptr, conn] : connections)
            conns.push_back(conn);
    }

    for (auto& conn : conns)
    {
        conn->getEventBase()->runInEventBaseThreadAndWait([&conn]()
                                                          {
                                                              conn->assertOnEvb();
                                                              conn->close(); });
    }

    {
        CondVarLock lock(tasksMutex);
        while (runningTasks != 0)
            lock.wait(tasksCond);
    }

    conns.clear();

    ioPool->stop();
    ioPool->join();
    ioPool.reset();
    acceptors.clear();
    evbs.clear();
}

void RedisServer::onAccepted(folly::EventBase* evb, folly::NetworkSocket fd, const folly::SocketAddress& clientAddr)
{
    auto sock = folly::AsyncSocket::newSocket(evb, fd, &clientAddr);
    sock->setNoDelay(true);

    const auto id = nextConnectionId++;
    auto conn = std::make_shared<RedisConnection>(*this, evb, std::move(sock), id);

    {
        std::scoped_lock lock(connectionsMutex);
        if (stopping)
            return;
        connections[conn.get()] = conn;
    }

    XLOGF(DBG0, "Redis connection {} from {}", id, clientAddr.describe());

    conn->assertOnEvb();
    conn->start();
}

void RedisServer::removeConnection(RedisConnection* conn)
{
    std::shared_ptr<RedisConnection> removed;
    std::scoped_lock lock(connectionsMutex);
    auto it = connections.find(conn);
    if (it != connections.end())
    {
        // Destroy after unlocking
        removed = std::move(it->second);
        connections.erase(it);
    }
}

void RedisServer::registerWaiter(const std::vector<std::string>& keys, const std::shared_ptr<RedisConnection>& conn)
{
    std::scoped_lock lock(waitersMutex);
    for (const auto& key : keys)
    {
        auto& keyWaiters = waiters[key];
        bool found = false;
        for (const auto& waiter : keyWaiters)
        {
            if (!waiter.owner_before(conn) && !conn.owner_before(waiter))
            {
                found = true;
                break;
            }
        }
        if (!found)
            keyWaiters.push_back(conn);
    }
}

void RedisServer::unregisterWaiter(const std::vector<std::string>& keys, const RedisConnection* conn)
{
    std::vector<std::shared_ptr<RedisConnection>> toRelease;
    std::scoped_lock lock(waitersMutex);
    for (const auto& key : keys)
    {
        auto it = waiters.find(key);
        if (it == waiters.end())
            continue;

        std::erase_if(it->second, [&](const std::weak_ptr<RedisConnection>& waiter)
                      {
                          auto locked = waiter.lock();
                          const bool ret = !locked || locked.get() == conn;
                          toRelease.push_back(std::move(locked));
                          return ret; });

        if (it->second.empty())
            waiters.erase(it);
    }
}

void RedisServer::notifyKey(const std::string& key)
{
    std::vector<std::weak_ptr<RedisConnection>> keyWaiters;
    {
        std::scoped_lock lock(waitersMutex);
        auto it = waiters.find(key);
        if (it == waiters.end())
            return;
        keyWaiters = std::move(it->second);
        waiters.erase(it);
    }

    for (auto& waiter : keyWaiters)
    {
        if (auto conn = waiter.lock())
            RedisConnection::wake(std::move(conn));
    }
}

std::optional<std::string> RedisServer::getBucketCtx(BucketCtx& ctx)
{
    const auto bucketInfo = buckets::getBucketInfo(bucketName);
    if (!bucketInfo)
        return resp::error(fmt::format("ERR bucket '{}' does not exist", bucketName));

    if (bucketInfo->versioning != buckets::VersioningState::Enabled)
        return resp::error(fmt::format("ERR versioning has to be enabled on bucket '{}'", bucketName));

    if (bucketInfo->objectLockEnabled)
        return resp::error(fmt::format("ERR bucket '{}' has object lock enabled, which is not supported", bucketName));

    ctx.id = bucketInfo->id;
    return std::nullopt;
}

std::optional<std::string> RedisServer::checkAuth(const ConnState& state, const std::string_view cmd, const std::initializer_list<Action> actions,
                                                  const std::optional<std::string_view> key)
{
    const auto resource = key ? fmt::format("arn:aws:s3:::{}/{}", bucketName, *key) : fmt::format("arn:aws:s3:::{}", bucketName);
    for (const auto action : actions)
    {
        if (!isAuthorized(resource, action, state.accessKey))
        {
            if (state.accessKey.empty())
                return resp::error("NOAUTH Authentication required.");

            return resp::error(fmt::format("NOPERM User {} has no permissions to run the '{}' command on '{}'", state.accessKey, toLower(cmd), key ? *key : std::string_view()));
        }
    }
    return std::nullopt;
}

std::optional<std::string> RedisServer::checkKey(const std::string_view key)
{
    if (key.empty())
        return resp::error("ERR empty keys are not supported");
    if (key.size() > maxKeyLen)
        return resp::error(fmt::format("ERR key too long (max {} bytes)", maxKeyLen));
    return std::nullopt;
}

bool RedisServer::listEntries(const int64_t bucketId, const std::string& key, std::vector<Entry>& entries, const size_t maxElements)
{
    SingleFileStorage::ListIterData iterData;
    SCOPE_EXIT { sfs.list_iter_stop(iterData); };

    const auto start = make_key(key, bucketId, std::numeric_limits<int64_t>::max());
    if (!sfs.list_iter_start(start, start, [bucketId, key](const std::string& iterKey)
                             {
                                 const auto keyInfo = extractKeyInfoView(iterKey);
                                 return keyInfo.bucketId == bucketId && keyInfo.key == key; }, iterData))
    {
        return false;
    }

    size_t elements = 0;
    while (elements < maxElements)
    {
        std::string keyBin, md5sum;
        int64_t offset, size, lastModified;
        std::vector<SingleFileStorage::SPunchItem> extraExts;
        if (!sfs.iter_curr_val(keyBin, offset, size, extraExts, lastModified, md5sum, iterData.curr))
            break;

        const auto keyInfo = extractKeyInfoView(keyBin);
        if (keyInfo.bucketId != bucketId || keyInfo.key != key)
            break;

        const auto tombstone = isTombstone(md5sum);
        entries.push_back(Entry{ .version = keyInfo.version, .tombstone = tombstone, .isList = isListElement(md5sum) });
        if (!tombstone)
            ++elements;

        if (!sfs.list_iter_next(iterData))
            return false;
    }

    return true;
}

std::optional<RedisServer::Entry> RedisServer::findEnd(const int64_t bucketId, const std::string& key, const bool newest, int& err)
{
    const auto fn = make_key(key, bucketId, newest ? std::numeric_limits<int64_t>::max() : std::numeric_limits<int64_t>::min());
    const auto res = sfs.read_prepare(fn, (newest ? SingleFileStorage::ReadNewest : SingleFileStorage::ReadOldest) | SingleFileStorage::ReadSkipAddReading);
    if (res.err == ENOENT)
        return std::nullopt;
    if (res.err != 0)
    {
        err = res.err;
        return std::nullopt;
    }

    return Entry{ .version = extractKeyInfoView(res.key).version, .tombstone = isTombstone(res.md5sum), .isList = isListElement(res.md5sum) };
}

std::optional<RedisServer::Entry> RedisServer::findElement(const int64_t bucketId, const std::string& key, const bool newest, int& err)
{
    const auto entry = findEnd(bucketId, key, newest, err);
    if (!entry || !entry->tombstone)
        return entry;

    // Skip delete markers (rare)
    std::vector<Entry> entries;
    if (!listEntries(bucketId, key, entries, newest ? 1 : std::numeric_limits<size_t>::max()))
    {
        err = EIO;
        return std::nullopt;
    }

    if (newest)
    {
        for (const auto& curr : entries)
        {
            if (!curr.tombstone)
                return curr;
        }
    }
    else
    {
        for (auto it = entries.rbegin(); it != entries.rend(); ++it)
        {
            if (!it->tombstone)
                return *it;
        }
    }

    return std::nullopt;
}

RedisServer::ReadRes RedisServer::readFromReader(std::shared_ptr<ObjectReader> reader, const std::string& key, const ReadMode mode)
{
    ReadRes ret{ .status = ReadRes::Status::Ok, .size = reader->FileSize() };
    if (mode == ReadMode::Size)
        return ret;

    if (ret.size > FLAGS_redis_max_value_size)
    {
        return ReadRes{ .status = ReadRes::Status::Error,
                        .value = fmt::format("value of key '{}' is too large ({} bytes, max {}). Read it via S3", key, ret.size, FLAGS_redis_max_value_size) };
    }

    const auto size = static_cast<size_t>(ret.size);
    if (mode == ReadMode::Bulk)
    {
        if (ret.size >= streamThreshold)
        {
            // Read while sending the reply. The reader keeps the data readable even if the version is deleted
            ret.reader = std::move(reader);
            return ret;
        }

        const auto header = fmt::format("${}\r\n", size);
        ret.value.resize(header.size() + size + 2);
        std::memcpy(ret.value.data(), header.data(), header.size());
        if (size > 0)
            reader->Read(ret.value.data() + header.size(), ret.size, 0);
        ret.value[header.size() + size] = '\r';
        ret.value[header.size() + size + 1] = '\n';
    }
    else if (size > 0)
    {
        ret.value.resize(size);
        reader->Read(ret.value.data(), ret.size, 0);
    }
    return ret;
}

RedisServer::ReadRes RedisServer::readValue(const std::string& key, const int64_t version, const ReadMode mode)
{
    try
    {
        return readFromReader(std::make_shared<ObjectReader>(sfs, bucketName, key, version, ObjectReader::VersionSelect::Exact), key, mode);
    }
    catch (const ObjectReadError& e)
    {
        if (e.notFound())
            return ReadRes{ .status = ReadRes::Status::NotFound };

        XLOGF(WARN, "Error reading Redis key {} version {}: {}", key, version, e.what());
        return ReadRes{ .status = ReadRes::Status::Error, .value = e.what() };
    }
}

RedisServer::ReadRes RedisServer::getString(const std::string& key, const ReadMode mode)
{
    try
    {
        // Newest version (one lookup). Not found if it is a delete marker
        auto reader = std::make_shared<ObjectReader>(sfs, bucketName, key);
        if (isListReader(*reader))
            return ReadRes{ .status = ReadRes::Status::WrongType };

        return readFromReader(std::move(reader), key, mode);
    }
    catch (const ObjectReadError& e)
    {
        if (e.notFound())
            return ReadRes{ .status = ReadRes::Status::NotFound };

        XLOGF(WARN, "Error reading Redis key {}: {}", key, e.what());
        return ReadRes{ .status = ReadRes::Status::Error, .value = e.what() };
    }
}

int RedisServer::prepareValue(const int64_t bucketId, const std::string& key, const ValueArg& value, const ContentType& contentType, PreparedValue& prepared)
{
    ObjMetadata objMetadata;
    objMetadata.contentType = contentType;

    CWData metadata;
    metadata.addVarInt(metadata_object | objMetadata.flags());
    if (FLAGS_disable_md5_etag)
    {
        const auto versionEtag = sfs.get_next_version_etag();
        if (versionEtag.size() != MD5_DIGEST_LENGTH)
            return EIO;
        metadata.addBuffer(versionEtag.data(), MD5_DIGEST_LENGTH);
    }
    else if (value.streamed)
    {
        // Command runs on the serial executor after the value was written completely
        value.streamed->assertWriter();
        if (value.streamed->md5sum.size() != MD5_DIGEST_LENGTH)
            return value.streamed->err != 0 ? value.streamed->err : EIO;
        metadata.addBuffer(value.streamed->md5sum.data(), MD5_DIGEST_LENGTH);
    }
    else
    {
        unsigned char md5[EVP_MAX_MD_SIZE];
        unsigned int md5Len = 0;
        if (EVP_Digest(value.data.data(), value.data.size(), md5, &md5Len, EVP_md5(), nullptr) != 1 || md5Len != MD5_DIGEST_LENGTH)
            return EIO;
        metadata.addBuffer(reinterpret_cast<const char*>(md5), MD5_DIGEST_LENGTH);
    }
    objMetadata.serialize(metadata);
    prepared.metadata.assign(metadata.getDataPtr(), metadata.getDataSize());

    if (value.streamed)
    {
        auto& stream = *value.streamed;
        stream.assertWriter();
        if (stream.err != 0)
            return stream.err;
        // Can only be used once
        if (!stream.finished || stream.extents.empty())
            return EIO;

        prepared.extents = std::move(stream.extents);
        stream.extents.clear();
        return 0;
    }

    if (value.data.empty())
        return 0;

    // Version is allocated when the value is added as object. Use the version with the longest encoding
    auto res = sfs.write_prepare(make_key(key, bucketId, std::numeric_limits<int64_t>::min()), value.data.size());
    if (res.err != 0)
        return res.err;

    prepared.extents = std::move(res.extents);
    return writeToExtents(sfs, prepared.extents, 0, value.data.data(), value.data.size());
}

int RedisServer::deleteVersion(const int64_t bucketId, const std::string& key, const int64_t version, const bool requireExisting, bool& deleted)
{
    SingleFileStorage::MatchInfo matchInfo;
    if (requireExisting)
        matchInfo.ifMatch.push_back("*");

    const auto rc = sfs.del(make_key(key, bucketId, version), SingleFileStorage::DelAction::Del, false, nullptr, requireExisting ? &matchInfo : nullptr);
    deleted = rc == 0 && (!requireExisting || matchInfo.match);
    return rc;
}

int RedisServer::setValue(const int64_t bucketId, const std::string& key, const ValueArg& value)
{
    PreparedValue prepared(sfs);
    auto rc = prepareValue(bucketId, key, value, stringContentType(), prepared);
    if (rc != 0)
        return rc;

    // Strings are stored as unversioned object (version 0). It is the value of the key if there are no other
    // versions: Positive versions (list elements, S3 versions, delete markers) sort before it. Negative versions
    // (list elements) only exist after it if the newest version is a list element (pushes check the type)
    const auto fn = make_key(key, bucketId, 0);
    SingleFileStorage::MatchInfo matchInfo;
    matchInfo.readFn = make_key(key, bucketId, std::numeric_limits<int64_t>::max());
    matchInfo.readNewest = true;
    matchInfo.ifVersion = 0;
    rc = prepared.add(fn, &matchInfo);
    if (rc != 0 || matchInfo.match)
        return rc;

    // Key has other versions. Replace them
    rc = prepared.add(fn, nullptr);
    if (rc != 0)
        return rc;

    std::vector<Entry> entries;
    if (!listEntries(bucketId, key, entries))
        return EIO;

    for (const auto& entry : entries)
    {
        if (entry.version != 0)
        {
            bool deleted;
            rc = deleteVersion(bucketId, key, entry.version, false, deleted);
            if (rc != 0)
                return rc;
        }
    }

    return 0;
}

RedisServer::PopStatus RedisServer::popElement(const int64_t bucketId, const std::string& key, const bool left, ReadRes& value, std::string& err)
{
    for (int i = 0; i < maxRetries; ++i)
    {
        int rc = 0;
        const auto entry = findElement(bucketId, key, left, rc);
        if (rc != 0)
        {
            err = fmt::format("ERR storage error {}", rc);
            return PopStatus::Error;
        }

        if (!entry)
            return PopStatus::Empty;

        if (!entry->isList)
            return PopStatus::WrongType;

        // Large values are read while sending the reply (also after the version is deleted)
        auto res = readValue(key, entry->version, ReadMode::Bulk);
        if (res.status == ReadRes::Status::NotFound)
            continue;

        if (res.status == ReadRes::Status::Error)
        {
            err = fmt::format("ERR error reading value: {}", res.value);
            return PopStatus::Error;
        }

        bool deleted;
        rc = deleteVersion(bucketId, key, entry->version, true, deleted);
        if (rc != 0)
        {
            err = fmt::format("ERR storage error {}", rc);
            return PopStatus::Error;
        }

        if (!deleted)
            continue;

        value = std::move(res);
        return PopStatus::Popped;
    }

    err = "ERR too many concurrent modifications";
    return PopStatus::Error;
}

int64_t RedisServer::listLength(const int64_t bucketId, const std::string& key, int& err)
{
    const auto res = sfs.read_prepare(make_key(key, buckets::getCountersBucket(bucketId), 0), SingleFileStorage::ReadSkipAddReading);
    if (res.err == ENOENT)
        return 0;
    if (res.err != 0)
    {
        err = res.err;
        return 0;
    }

    CRData rdata(res.md5sum.data(), res.md5sum.size());
    int64_t count;
    if (!rdata.getVarInt(&count))
    {
        err = EIO;
        return 0;
    }
    return count;
}

void RedisServer::setupCounters(SingleFileStorage::SFSOptions& options)
{
    // Number of list elements per key in Redis buckets, maintained by the storage engine (also for modifications via
    // S3 and if the Redis server is disabled)
    options.counter_key_callback = [](const std::string& fn) -> std::string
    {
        const auto keyInfo = extractKeyInfoView(fn);
        if (!buckets::isRedisBucket(keyInfo.bucketId))
            return std::string();
        return make_key(keyInfo.key, buckets::getCountersBucket(keyInfo.bucketId), 0);
    };
    options.counter_counts_callback = [](const std::string& /*fn*/, const std::string& md5sum)
    {
        return isListElement(md5sum);
    };
}

bool RedisServer::commit()
{
    return S3Handler::commitStorage(sfs);
}

template <typename F>
bool RedisServer::iterateKeys(const int64_t bucketId, const std::optional<std::string>& startAfter, const std::string& prefix, F f, bool& atEnd)
{
    atEnd = false;

    const auto start = startAfter && *startAfter >= prefix ? make_key(*startAfter, bucketId, std::numeric_limits<int64_t>::min())
                                                           : make_key(prefix, bucketId, std::numeric_limits<int64_t>::max());

    SingleFileStorage::ListIterData iterData;
    SCOPE_EXIT { sfs.list_iter_stop(iterData); };

    if (!sfs.list_iter_start(start, start, [bucketId, prefix](const std::string& iterKey)
                             {
                                 const auto keyInfo = extractKeyInfoView(iterKey);
                                 return keyInfo.bucketId == bucketId && keyInfo.key.starts_with(prefix); }, iterData))
    {
        return false;
    }

    // Versions of a key are newest first, so the first entry of a key determines whether it is live
    std::optional<std::string> prevKey = startAfter;
    while (true)
    {
        std::string keyBin, md5sum;
        int64_t offset, size, lastModified;
        std::vector<SingleFileStorage::SPunchItem> extraExts;
        if (!sfs.iter_curr_val(keyBin, offset, size, extraExts, lastModified, md5sum, iterData.curr))
        {
            atEnd = true;
            return true;
        }

        const auto keyInfo = extractKeyInfoView(keyBin);
        if (keyInfo.bucketId != bucketId || !keyInfo.key.starts_with(prefix))
        {
            atEnd = true;
            return true;
        }

        if (!prevKey || *prevKey != keyInfo.key)
        {
            prevKey = std::string(keyInfo.key);
            const auto type = isTombstone(md5sum) ? KeyType::None : (isListElement(md5sum) ? KeyType::List : KeyType::String);
            if (!f(*prevKey, type))
                return true;
        }

        if (!sfs.list_iter_next(iterData))
            return false;
    }
}

RedisServer::ExecResult RedisServer::exec(ConnState& state, const RedisCommand& command, const std::shared_ptr<RedisConnection>& conn)
{
    const auto& args = command.args;
    assert(!args.empty());
    const auto cmd = toUpper(args[0]);
    const auto argc = args.size();

    XLOGF(DBG0, "Redis connection {} command {} argc {}", state.id, cmd, argc);

    if (cmd == "PING")
    {
        if (argc == 1)
            return { resp::simple("PONG") };
        if (argc == 2)
            return { resp::bulk(args[1]) };
        return { resp::wrongArgs("ping") };
    }
    if (cmd == "ECHO")
    {
        if (argc != 2)
            return { resp::wrongArgs("echo") };
        return { resp::bulk(args[1]) };
    }
    if (cmd == "QUIT")
        return { .reply = resp::ok, .close = true };
    if (cmd == "AUTH")
        return cmdAuth(state, args);
    if (cmd == "HELLO")
        return cmdHello(state, args);
    if (cmd == "SELECT")
    {
        if (argc != 2)
            return { resp::wrongArgs("select") };
        int64_t db;
        if (!parseInt64(args[1], db))
            return { resp::notInteger };
        if (db != 0)
            return { resp::error("ERR DB index is out of range") };
        return { resp::ok };
    }
    if (cmd == "CLIENT")
        return cmdClient(state, args);
    if (cmd == "COMMAND")
    {
        if (argc >= 2 && toUpper(args[1]) == "COUNT")
            return { resp::integer(0) };
        return { resp::emptyArray };
    }
    if (cmd == "INFO")
    {
        return { resp::bulk(fmt::format("# Server\r\nredis_version:{}\r\nredis_mode:standalone\r\nhs5_bucket:{}\r\n", redisVersion, bucketName)) };
    }
    if (cmd == "RESET")
    {
        state.accessKey.clear();
        state.name.reset();
        state.resp3 = false;
        return { resp::simple("RESET") };
    }
    if (cmd == "GET")
        return cmdGet(state, args);
    if (cmd == "STRLEN")
        return cmdStrlen(state, args);
    if (cmd == "MGET")
        return cmdMget(state, args);
    if (cmd == "SET")
        return cmdSet(state, command);
    if (cmd == "SETNX")
        return cmdSetnx(state, command);
    if (cmd == "MSET")
        return cmdMset(state, command);
    if (cmd == "INCR" || cmd == "DECR" || cmd == "INCRBY" || cmd == "DECRBY")
        return cmdIncr(state, args, cmd);
    if (cmd == "DEL" || cmd == "UNLINK")
        return cmdDel(state, args);
    if (cmd == "EXISTS")
        return cmdExists(state, args);
    if (cmd == "TYPE")
        return cmdType(state, args);
    if (cmd == "LPUSH")
        return cmdPush(state, command, true, false);
    if (cmd == "RPUSH")
        return cmdPush(state, command, false, false);
    if (cmd == "LPUSHX")
        return cmdPush(state, command, true, true);
    if (cmd == "RPUSHX")
        return cmdPush(state, command, false, true);
    if (cmd == "LPOP")
        return cmdPop(state, args, true);
    if (cmd == "RPOP")
        return cmdPop(state, args, false);
    if (cmd == "BLPOP")
        return cmdBlockingPop(state, args, true, conn);
    if (cmd == "BRPOP")
        return cmdBlockingPop(state, args, false, conn);
    if (cmd == "LLEN")
        return cmdLlen(state, args);
    if (cmd == "LRANGE")
        return cmdLrange(state, args);
    if (cmd == "LINDEX")
        return cmdLindex(state, args);
    if (cmd == "SCAN")
        return cmdScan(state, args);
    if (cmd == "KEYS")
        return cmdKeys(state, args);
    if (cmd == "DBSIZE")
        return cmdDbsize(state, args);
    if (cmd == "FLUSHDB" || cmd == "FLUSHALL")
        return cmdFlushdb(state, args);

    std::string argsStr;
    for (size_t i = 1; i < argc && argsStr.size() < 128; ++i)
        argsStr += fmt::format("'{}' ", args[i].substr(0, 128));
    return { resp::error(fmt::format("ERR unknown command '{}', with args beginning with: {}", args[0].substr(0, 128), argsStr)) };
}

std::optional<RedisServer::ExecResult> RedisServer::execInline(ConnState& state, const RedisCommand& command)
{
    const auto& args = command.args;
    if (args.size() != 2 || args[0].size() != 3 || toUpper(args[0]) != "GET")
        return std::nullopt;

    const auto& key = args[1];
    if (checkKey(key) || checkAuth(state, args[0], { Action::GetObject }, key))
        return std::nullopt;
    BucketCtx ctx;
    if (getBucketCtx(ctx))
        return std::nullopt;

    try
    {
        // Newest version (one lookup). Not found if it is a delete marker
        // Reads of keys with uncommitted modifications would wait for the commit thread. Execute them via exec instead
        ObjectReader reader(sfs, bucketName, key, 0, ObjectReader::VersionSelect::Default, SingleFileStorage::ReadWithMmap,
                            SingleFileStorage::ReadFailUnsynced);
        if (isListReader(reader))
            return ExecResult{ resp::wrongType };

        const auto size = reader.FileSize();
        if (size >= FLAGS_get_in_worker_threshold)
            return std::nullopt;

        const auto header = fmt::format("${}\r\n", size);
        std::string reply;
        reply.resize(header.size() + static_cast<size_t>(size) + 2);
        std::memcpy(reply.data(), header.data(), header.size());
        if (size > 0)
            reader.Read(reply.data() + header.size(), size, 0);
        reply[header.size() + static_cast<size_t>(size)] = '\r';
        reply[header.size() + static_cast<size_t>(size) + 1] = '\n';
        return ExecResult{ std::move(reply) };
    }
    catch (const ObjectReadError& e)
    {
        if (e.notFound())
            return ExecResult{ resp::nullBulk(state) };
        // Error is reported via exec
        return std::nullopt;
    }
}

RedisServer::ExecResult RedisServer::cmdAuth(ConnState& state, const std::vector<std::string>& args)
{
    if (args.size() == 2)
        return { resp::error("WRONGPASS invalid username-password pair or user is disabled. Use AUTH <access key> <secret key>") };
    if (args.size() != 3)
        return { resp::wrongArgs("auth") };

    const auto secretKey = getSecretKey(args[1]);
    const auto& password = args[2];
    if (secretKey.empty() || secretKey.size() != password.size() ||
        sodium_memcmp(secretKey.data(), password.data(), password.size()) != 0)
    {
        XLOGF(INFO, "Redis connection {} authentication failed for access key {}", state.id, args[1]);
        return { resp::error("WRONGPASS invalid username-password pair or user is disabled.") };
    }

    state.accessKey = args[1];
    return { resp::ok };
}

RedisServer::ExecResult RedisServer::cmdHello(ConnState& state, const std::vector<std::string>& args)
{
    size_t idx = 1;
    if (args.size() >= 2)
    {
        int64_t protover;
        if (!parseInt64(args[1], protover) || protover < 1)
            return { resp::error("ERR Protocol version is not an integer or out of range") };
        if (protover != 2 && protover != 3)
            return { resp::error("NOPROTO unsupported protocol version") };
        idx = 2;
    }
    const auto protover = args.size() >= 2 ? std::stoi(args[1]) : (state.resp3 ? 3 : 2);

    std::optional<std::string> name;
    for (; idx < args.size(); ++idx)
    {
        const auto opt = toUpper(args[idx]);
        if (opt == "AUTH" && idx + 2 < args.size())
        {
            auto res = cmdAuth(state, { "AUTH", args[idx + 1], args[idx + 2] });
            if (res.reply != resp::ok)
                return res;
            idx += 2;
        }
        else if (opt == "SETNAME" && idx + 1 < args.size())
        {
            name = args[idx + 1];
            ++idx;
        }
        else
        {
            return { resp::error(fmt::format("ERR Syntax error in HELLO option '{}'", args[idx])) };
        }
    }

    if (name)
        state.name = name;
    state.resp3 = protover == 3;

    std::string reply = state.resp3 ? "%7\r\n" : resp::arrayHeader(14);
    resp::appendBulk(reply, "server");
    resp::appendBulk(reply, "redis");
    resp::appendBulk(reply, "version");
    resp::appendBulk(reply, redisVersion);
    resp::appendBulk(reply, "proto");
    reply += resp::integer(protover);
    resp::appendBulk(reply, "id");
    reply += resp::integer(state.id);
    resp::appendBulk(reply, "mode");
    resp::appendBulk(reply, "standalone");
    resp::appendBulk(reply, "role");
    resp::appendBulk(reply, "master");
    resp::appendBulk(reply, "modules");
    reply += resp::emptyArray;
    return { reply };
}

RedisServer::ExecResult RedisServer::cmdClient(ConnState& state, const std::vector<std::string>& args)
{
    if (args.size() < 2)
        return { resp::wrongArgs("client") };

    const auto sub = toUpper(args[1]);
    if (sub == "SETNAME" && args.size() == 3)
    {
        state.name = args[2];
        return { resp::ok };
    }
    if (sub == "GETNAME" && args.size() == 2)
        return { state.name ? resp::bulk(*state.name) : resp::nullBulk(state) };
    if (sub == "ID" && args.size() == 2)
        return { resp::integer(state.id) };
    if (sub == "SETINFO" && args.size() == 4)
        return { resp::ok };

    return { resp::error(fmt::format("ERR unknown subcommand or wrong number of arguments for '{}'", args[1])) };
}

RedisServer::ExecResult RedisServer::cmdGet(ConnState& state, const std::vector<std::string>& args)
{
    if (args.size() != 2)
        return { resp::wrongArgs("get") };

    const auto& key = args[1];
    if (auto err = checkKey(key))
        return { *err };
    if (auto err = checkAuth(state, args[0], { Action::GetObject }, key))
        return { *err };
    BucketCtx ctx;
    if (auto err = getBucketCtx(ctx))
        return { *err };

    auto res = getString(key, ReadMode::Bulk);
    if (res.status == ReadRes::Status::NotFound)
        return { resp::nullBulk(state) };
    if (res.status == ReadRes::Status::WrongType)
        return { resp::wrongType };
    if (res.status == ReadRes::Status::Error)
        return { resp::error(fmt::format("ERR {}", res.value)) };
    ReplyBuilder reply;
    reply.appendBulk(res);
    return reply.finish();
}

RedisServer::ExecResult RedisServer::cmdStrlen(ConnState& state, const std::vector<std::string>& args)
{
    if (args.size() != 2)
        return { resp::wrongArgs("strlen") };

    const auto& key = args[1];
    if (auto err = checkKey(key))
        return { *err };
    if (auto err = checkAuth(state, args[0], { Action::GetObject }, key))
        return { *err };
    BucketCtx ctx;
    if (auto err = getBucketCtx(ctx))
        return { *err };

    const auto res = getString(key, ReadMode::Size);
    if (res.status == ReadRes::Status::NotFound)
        return { resp::integer(0) };
    if (res.status == ReadRes::Status::WrongType)
        return { resp::wrongType };
    if (res.status == ReadRes::Status::Error)
        return { resp::error(fmt::format("ERR {}", res.value)) };
    return { resp::integer(res.size) };
}

RedisServer::ExecResult RedisServer::cmdMget(ConnState& state, const std::vector<std::string>& args)
{
    if (args.size() < 2)
        return { resp::wrongArgs("mget") };

    for (size_t i = 1; i < args.size(); ++i)
    {
        if (auto err = checkKey(args[i]))
            return { *err };
        if (auto err = checkAuth(state, args[0], { Action::GetObject }, args[i]))
            return { *err };
    }
    BucketCtx ctx;
    if (auto err = getBucketCtx(ctx))
        return { *err };

    // Atomic with respect to MSET/DEL
    MultiKeyLock lock(*this, args, 1, 1, false);

    ReplyBuilder reply;
    reply.append(resp::arrayHeader(args.size() - 1));
    for (size_t i = 1; i < args.size(); ++i)
    {
        auto res = getString(args[i], ReadMode::Bulk);
        if (res.status == ReadRes::Status::Error)
            return { resp::error(fmt::format("ERR {}", res.value)) };
        if (res.status == ReadRes::Status::NotFound || res.status == ReadRes::Status::WrongType)
            reply.append(resp::nullBulk(state));
        else
            reply.appendBulk(res);
    }
    return reply.finish();
}

RedisServer::ExecResult RedisServer::cmdSet(ConnState& state, const RedisCommand& command)
{
    const auto& args = command.args;
    if (args.size() < 3)
        return { resp::wrongArgs("set") };

    bool nx = false, xx = false, get = false;
    for (size_t i = 3; i < args.size(); ++i)
    {
        const auto opt = toUpper(args[i]);
        if (opt == "NX")
            nx = true;
        else if (opt == "XX")
            xx = true;
        else if (opt == "GET")
            get = true;
        else if (opt == "KEEPTTL")
        {
        }
        else if (opt == "EX" || opt == "PX" || opt == "EXAT" || opt == "PXAT")
            return { resp::error("ERR key expiration (EX, PX, EXAT, PXAT) is not supported") };
        else
            return { resp::syntaxError };
    }

    if (nx && xx)
        return { resp::syntaxError };

    const auto& key = args[1];
    if (auto err = checkKey(key))
        return { *err };
    if (auto err = checkAuth(state, args[0], { Action::PutObject }, key))
        return { *err };
    if (get || nx || xx)
    {
        if (auto err = checkAuth(state, args[0], { Action::GetObject }, key))
            return { *err };
    }
    BucketCtx ctx;
    if (auto err = getBucketCtx(ctx))
        return { *err };

    ExecResult reply;
    {
        std::scoped_lock lock(keyLock(key));

        // Large old values are read while sending the reply (also after the old version is deleted)
        ReadRes old{ .status = ReadRes::Status::NotFound };
        if (get || nx || xx)
        {
            old = getString(key, get ? ReadMode::Bulk : ReadMode::Size);
            if (old.status == ReadRes::Status::Error)
                return { resp::error(fmt::format("ERR {}", old.value)) };
            if (get && old.status == ReadRes::Status::WrongType)
                return { resp::wrongType };
        }

        const bool exists = old.status == ReadRes::Status::Ok || old.status == ReadRes::Status::WrongType;
        const auto getReply = [&]()
        {
            if (!exists)
                return ExecResult{ resp::nullBulk(state) };
            ReplyBuilder builder;
            builder.appendBulk(old);
            return builder.finish();
        };

        if ((nx && exists) || (xx && !exists))
            return get ? getReply() : ExecResult{ resp::nullBulk(state) };

        const auto rc = setValue(ctx.id, key, command.valueArg(2));
        if (rc != 0)
            return { resp::error(fmt::format("ERR storage error {}", rc)) };

        reply = get ? getReply() : ExecResult{ resp::ok };
    }

    if (!commit())
        return { resp::error("ERR storage commit failed") };

    return reply;
}

RedisServer::ExecResult RedisServer::cmdSetnx(ConnState& state, const RedisCommand& command)
{
    const auto& args = command.args;
    if (args.size() != 3)
        return { resp::wrongArgs("setnx") };

    auto res = cmdSet(state, RedisCommand{ .args = { "SET", args[1], args[2], "NX" }, .streamed = { nullptr, nullptr, command.streamedArg(2), nullptr } });
    if (res.reply == resp::ok)
        res.reply = resp::integer(1);
    else if (res.reply == resp::nullBulk(state))
        res.reply = resp::integer(0);
    return res;
}

RedisServer::ExecResult RedisServer::cmdMset(ConnState& state, const RedisCommand& command)
{
    const auto& args = command.args;
    if (args.size() < 3 || args.size() % 2 != 1)
        return { resp::wrongArgs("mset") };

    for (size_t i = 1; i < args.size(); i += 2)
    {
        if (auto err = checkKey(args[i]))
            return { *err };
        if (auto err = checkAuth(state, args[0], { Action::PutObject }, args[i]))
            return { *err };
    }
    BucketCtx ctx;
    if (auto err = getBucketCtx(ctx))
        return { *err };

    {
        // Atomic: MGET/EXISTS of these keys wait until all are set
        MultiKeyLock lock(*this, args, 1, 2, true);
        for (size_t i = 1; i < args.size(); i += 2)
        {
            const auto rc = setValue(ctx.id, args[i], command.valueArg(i + 1));
            if (rc != 0)
                return { resp::error(fmt::format("ERR storage error {}", rc)) };
        }
    }

    if (!commit())
        return { resp::error("ERR storage commit failed") };

    return { resp::ok };
}

RedisServer::ExecResult RedisServer::cmdIncr(ConnState& state, const std::vector<std::string>& args, const std::string& cmd)
{
    const bool withIncrement = cmd == "INCRBY" || cmd == "DECRBY";
    if (args.size() != (withIncrement ? 3 : 2))
        return { resp::wrongArgs(toLower(cmd)) };

    int64_t increment = 1;
    if (withIncrement && !parseInt64(args[2], increment))
        return { resp::notInteger };

    if (cmd == "DECR" || cmd == "DECRBY")
    {
        if (increment == std::numeric_limits<int64_t>::min())
            return { resp::error("ERR decrement would overflow") };
        increment = -increment;
    }

    const auto& key = args[1];
    if (auto err = checkKey(key))
        return { *err };
    if (auto err = checkAuth(state, args[0], { Action::GetObject, Action::PutObject }, key))
        return { *err };
    BucketCtx ctx;
    if (auto err = getBucketCtx(ctx))
        return { *err };

    int64_t newVal;
    {
        std::scoped_lock lock(keyLock(key));

        // Don't read large values into memory, they are no integers
        const auto currSize = getString(key, ReadMode::Size);
        if (currSize.status == ReadRes::Status::Error)
            return { resp::error(fmt::format("ERR {}", currSize.value)) };
        if (currSize.status == ReadRes::Status::WrongType)
            return { resp::wrongType };
        if (currSize.status == ReadRes::Status::Ok && currSize.size > 20)
            return { resp::notInteger };

        const auto curr = getString(key, ReadMode::Value);
        if (curr.status == ReadRes::Status::Error)
            return { resp::error(fmt::format("ERR {}", curr.value)) };

        int64_t currVal = 0;
        if (curr.status == ReadRes::Status::Ok && !parseInt64(curr.value, currVal))
            return { resp::notInteger };

        if (__builtin_add_overflow(currVal, increment, &newVal))
            return { resp::error("ERR increment or decrement would overflow") };

        const auto newValStr = std::to_string(newVal);
        const auto rc = setValue(ctx.id, key, ValueArg{ .data = newValStr });
        if (rc != 0)
            return { resp::error(fmt::format("ERR storage error {}", rc)) };
    }

    if (!commit())
        return { resp::error("ERR storage commit failed") };

    return { resp::integer(newVal) };
}

RedisServer::ExecResult RedisServer::cmdDel(ConnState& state, const std::vector<std::string>& args)
{
    if (args.size() < 2)
        return { resp::wrongArgs(toLower(args[0])) };

    for (size_t i = 1; i < args.size(); ++i)
    {
        if (auto err = checkKey(args[i]))
            return { *err };
        if (auto err = checkAuth(state, args[0], { Action::DeleteObject }, args[i]))
            return { *err };
    }
    BucketCtx ctx;
    if (auto err = getBucketCtx(ctx))
        return { *err };

    int64_t numDeleted = 0;
    bool modified = false;
    {
        // Atomic: MGET/EXISTS of these keys wait until all are deleted
        MultiKeyLock lock(*this, args, 1, 1, true);
        for (size_t i = 1; i < args.size(); ++i)
        {
            const auto& key = args[i];

            std::vector<Entry> entries;
            if (!listEntries(ctx.id, key, entries))
                return { resp::error("ERR error listing versions") };

            bool existed = false;
            for (const auto& entry : entries)
            {
                if (!entry.tombstone)
                    existed = true;

                bool deleted;
                const auto rc = deleteVersion(ctx.id, key, entry.version, false, deleted);
                if (rc != 0)
                    return { resp::error(fmt::format("ERR storage error {}", rc)) };
                modified = true;
            }

            if (existed)
                ++numDeleted;
        }
    }

    if (modified && !commit())
        return { resp::error("ERR storage commit failed") };

    return { resp::integer(numDeleted) };
}

RedisServer::ExecResult RedisServer::cmdExists(ConnState& state, const std::vector<std::string>& args)
{
    if (args.size() < 2)
        return { resp::wrongArgs("exists") };

    for (size_t i = 1; i < args.size(); ++i)
    {
        if (auto err = checkKey(args[i]))
            return { *err };
        if (auto err = checkAuth(state, args[0], { Action::GetObject }, args[i]))
            return { *err };
    }
    BucketCtx ctx;
    if (auto err = getBucketCtx(ctx))
        return { *err };

    MultiKeyLock lock(*this, args, 1, 1, false);
    int64_t count = 0;
    for (size_t i = 1; i < args.size(); ++i)
    {
        int err = 0;
        const auto entry = findEnd(ctx.id, args[i], true, err);
        if (err != 0)
            return { resp::error(fmt::format("ERR storage error {}", err)) };
        if (entry && !entry->tombstone)
            ++count;
    }
    return { resp::integer(count) };
}

RedisServer::ExecResult RedisServer::cmdType(ConnState& state, const std::vector<std::string>& args)
{
    if (args.size() != 2)
        return { resp::wrongArgs("type") };

    const auto& key = args[1];
    if (auto err = checkKey(key))
        return { *err };
    if (auto err = checkAuth(state, args[0], { Action::GetObject }, key))
        return { *err };
    BucketCtx ctx;
    if (auto err = getBucketCtx(ctx))
        return { *err };

    int err = 0;
    const auto entry = findEnd(ctx.id, key, true, err);
    if (err != 0)
        return { resp::error(fmt::format("ERR storage error {}", err)) };
    if (!entry || entry->tombstone)
        return { resp::simple("none") };

    return { resp::simple(entry->isList ? "list" : "string") };
}

RedisServer::ExecResult RedisServer::cmdPush(ConnState& state, const RedisCommand& command, const bool left, const bool onlyIfExists)
{
    const auto& args = command.args;
    if (args.size() < 3)
        return { resp::wrongArgs(toLower(args[0])) };

    const auto& key = args[1];
    if (auto err = checkKey(key))
        return { *err };
    if (auto err = checkAuth(state, args[0], { Action::PutObject }, key))
        return { *err };
    if (onlyIfExists)
    {
        if (auto err = checkAuth(state, args[0], { Action::GetObject }, key))
            return { *err };
    }
    BucketCtx ctx;
    if (auto err = getBucketCtx(ctx))
        return { *err };

    {
        std::scoped_lock lock(keyLock(key));

        if (onlyIfExists)
        {
            int err = 0;
            const auto entry = findElement(ctx.id, key, true, err);
            if (err != 0)
                return { resp::error(fmt::format("ERR storage error {}", err)) };
            if (!entry)
                return { resp::integer(0) };
            if (!entry->isList)
                return { resp::wrongType };
        }

        for (size_t i = 2; i < args.size(); ++i)
        {
            PreparedValue prepared(sfs);
            auto rc = prepareValue(ctx.id, key, command.valueArg(i), listContentType(), prepared);
            if (rc != 0)
                return { resp::error(fmt::format("ERR storage error {}", rc)) };

            // Versions of prepended items are negative, so they sort after (are older than) all other versions
            const auto nextVersion = sfs.get_next_version();
            const auto version = left ? nextVersion : -nextVersion;

            // Key has to be a list (newest version is a list element) or not exist
            std::optional<SingleFileStorage::MatchInfo> matchInfo;
            if (i == 2)
            {
                matchInfo.emplace();
                matchInfo->readFn = make_key(key, ctx.id, std::numeric_limits<int64_t>::max());
                matchInfo->readNewest = true;
                matchInfo->ifContentType = listContentType().contentType;
            }

            rc = prepared.add(make_key(key, ctx.id, version), matchInfo ? &*matchInfo : nullptr);
            if (rc != 0)
                return { resp::error(fmt::format("ERR storage error {}", rc)) };
            if (matchInfo && !matchInfo->match)
                return { resp::wrongType };
        }
    }

    if (!commit())
        return { resp::error("ERR storage commit failed") };

    notifyKey(key);

    int err = 0;
    const auto len = listLength(ctx.id, key, err);
    if (err != 0)
        return { resp::error(fmt::format("ERR storage error {}", err)) };
    return { resp::integer(len) };
}

RedisServer::ExecResult RedisServer::cmdPop(ConnState& state, const std::vector<std::string>& args, const bool left)
{
    if (args.size() != 2 && args.size() != 3)
        return { resp::wrongArgs(toLower(args[0])) };

    std::optional<int64_t> count;
    if (args.size() == 3)
    {
        int64_t val;
        if (!parseInt64(args[2], val) || val < 0)
            return { resp::error("ERR value is out of range, must be positive") };
        count = val;
    }

    const auto& key = args[1];
    if (auto err = checkKey(key))
        return { *err };
    if (auto err = checkAuth(state, args[0], { Action::GetObject, Action::DeleteObject }, key))
        return { *err };
    BucketCtx ctx;
    if (auto err = getBucketCtx(ctx))
        return { *err };

    std::vector<ReadRes> values;
    std::string popErr;
    {
        std::scoped_lock lock(keyLock(key));

        if (count && *count == 0)
        {
            int err = 0;
            const auto entry = findElement(ctx.id, key, left, err);
            if (err != 0)
                return { resp::error(fmt::format("ERR storage error {}", err)) };
            if (entry && !entry->isList)
                return { resp::wrongType };
            return { entry ? resp::emptyArray : resp::nullArray(state) };
        }

        const int64_t toPop = count ? *count : 1;
        for (int64_t i = 0; i < toPop; ++i)
        {
            ReadRes value;
            const auto res = popElement(ctx.id, key, left, value, popErr);
            if (res == PopStatus::WrongType && values.empty())
                return { resp::wrongType };
            if (res != PopStatus::Popped)
                break;
            values.push_back(std::move(value));
        }
    }

    if (!values.empty() && !commit())
        return { resp::error("ERR storage commit failed") };

    // Report error only if nothing was popped (otherwise popped values would be lost)
    if (!popErr.empty() && values.empty())
        return { resp::error(popErr) };

    if (!count && values.empty())
        return { resp::nullBulk(state) };

    if (values.empty())
        return { resp::nullArray(state) };

    ReplyBuilder reply;
    if (count)
        reply.append(resp::arrayHeader(values.size()));
    for (auto& value : values)
        reply.appendBulk(value);
    return reply.finish();
}

RedisServer::ExecResult RedisServer::cmdBlockingPop(ConnState& state, const std::vector<std::string>& args, const bool left, const std::shared_ptr<RedisConnection>& conn)
{
    if (args.size() < 3)
        return { resp::wrongArgs(toLower(args[0])) };

    double timeout;
    if (!parseTimeout(args.back(), timeout))
        return { resp::error("ERR timeout is not a float or out of range") };
    if (timeout < 0)
        return { resp::error("ERR timeout is negative") };

    const std::vector<std::string> keys(args.begin() + 1, args.end() - 1);
    for (const auto& key : keys)
    {
        if (auto err = checkKey(key))
            return { *err };
        if (auto err = checkAuth(state, args[0], { Action::GetObject, Action::DeleteObject }, key))
            return { *err };
    }
    BucketCtx ctx;
    if (auto err = getBucketCtx(ctx))
        return { *err };

    // Register before trying to pop, so pushes in between wake up the connection
    registerWaiter(keys, conn);

    for (const auto& key : keys)
    {
        ReadRes value;
        std::string popErr;
        PopStatus res;
        {
            std::scoped_lock lock(keyLock(key));
            res = popElement(ctx.id, key, left, value, popErr);
        }

        if (res == PopStatus::Error)
        {
            unregisterWaiter(keys, conn.get());
            return { resp::error(popErr) };
        }

        if (res == PopStatus::WrongType)
        {
            unregisterWaiter(keys, conn.get());
            return { resp::wrongType };
        }

        if (res == PopStatus::Popped)
        {
            unregisterWaiter(keys, conn.get());

            if (!commit())
                return { resp::error("ERR storage commit failed") };

            ReplyBuilder reply;
            reply.append(resp::arrayHeader(2));
            reply.append(resp::bulk(key));
            reply.appendBulk(value);
            return reply.finish();
        }
    }

    return { .block = true, .blockTimeout = timeout };
}

RedisServer::ExecResult RedisServer::cmdLlen(ConnState& state, const std::vector<std::string>& args)
{
    if (args.size() != 2)
        return { resp::wrongArgs("llen") };

    const auto& key = args[1];
    if (auto err = checkKey(key))
        return { *err };
    if (auto err = checkAuth(state, args[0], { Action::GetObject }, key))
        return { *err };
    BucketCtx ctx;
    if (auto err = getBucketCtx(ctx))
        return { *err };

    int err = 0;
    const auto entry = findElement(ctx.id, key, true, err);
    if (err != 0)
        return { resp::error(fmt::format("ERR storage error {}", err)) };
    if (entry && !entry->isList)
        return { resp::wrongType };

    const auto len = listLength(ctx.id, key, err);
    if (err != 0)
        return { resp::error(fmt::format("ERR storage error {}", err)) };
    return { resp::integer(len) };
}

RedisServer::ExecResult RedisServer::cmdLrange(ConnState& state, const std::vector<std::string>& args)
{
    if (args.size() != 4)
        return { resp::wrongArgs("lrange") };

    int64_t start, stop;
    if (!parseInt64(args[2], start) || !parseInt64(args[3], stop))
        return { resp::notInteger };

    const auto& key = args[1];
    if (auto err = checkKey(key))
        return { *err };
    if (auto err = checkAuth(state, args[0], { Action::GetObject }, key))
        return { *err };
    BucketCtx ctx;
    if (auto err = getBucketCtx(ctx))
        return { *err };

    std::vector<Entry> entries;
    // Only list the necessary elements if indices are from the start
    const auto maxElements = start >= 0 && stop >= 0 ? static_cast<size_t>(stop) + 1 : std::numeric_limits<size_t>::max();
    if (!listEntries(ctx.id, key, entries, maxElements))
        return { resp::error("ERR error listing versions") };

    std::erase_if(entries, [](const Entry& e)
                  { return e.tombstone; });
    if (!entries.empty() && !entries.front().isList)
        return { resp::wrongType };

    const auto len = static_cast<int64_t>(entries.size());
    if (start < 0)
        start += len;
    if (stop < 0)
        stop += len;
    if (start < 0)
        start = 0;
    if (stop >= len)
        stop = len - 1;

    if (start > stop || start >= len)
        return { resp::emptyArray };

    std::vector<ReadRes> values;
    for (int64_t i = start; i <= stop; ++i)
    {
        auto res = readValue(key, entries[static_cast<size_t>(i)].version, ReadMode::Bulk);
        if (res.status == ReadRes::Status::Error)
            return { resp::error(fmt::format("ERR {}", res.value)) };
        // Skip elements deleted concurrently
        if (res.status == ReadRes::Status::Ok)
            values.push_back(std::move(res));
    }

    ReplyBuilder reply;
    reply.append(resp::arrayHeader(values.size()));
    for (auto& value : values)
        reply.appendBulk(value);
    return reply.finish();
}

RedisServer::ExecResult RedisServer::cmdLindex(ConnState& state, const std::vector<std::string>& args)
{
    if (args.size() != 3)
        return { resp::wrongArgs("lindex") };

    int64_t index;
    if (!parseInt64(args[2], index))
        return { resp::notInteger };

    const auto& key = args[1];
    if (auto err = checkKey(key))
        return { *err };
    if (auto err = checkAuth(state, args[0], { Action::GetObject }, key))
        return { *err };
    BucketCtx ctx;
    if (auto err = getBucketCtx(ctx))
        return { *err };

    std::vector<Entry> entries;
    if (!listEntries(ctx.id, key, entries, index >= 0 ? static_cast<size_t>(index) + 1 : std::numeric_limits<size_t>::max()))
        return { resp::error("ERR error listing versions") };

    std::erase_if(entries, [](const Entry& e)
                  { return e.tombstone; });
    if (!entries.empty() && !entries.front().isList)
        return { resp::wrongType };

    const auto len = static_cast<int64_t>(entries.size());
    if (index < 0)
        index += len;
    if (index < 0 || index >= len)
        return { resp::nullBulk(state) };

    auto res = readValue(key, entries[static_cast<size_t>(index)].version, ReadMode::Bulk);
    if (res.status == ReadRes::Status::Error)
        return { resp::error(fmt::format("ERR {}", res.value)) };
    if (res.status == ReadRes::Status::NotFound)
        return { resp::nullBulk(state) };
    ReplyBuilder reply;
    reply.appendBulk(res);
    return reply.finish();
}

RedisServer::ExecResult RedisServer::cmdScan(ConnState& state, const std::vector<std::string>& args)
{
    if (args.size() < 2 || args.size() % 2 != 0)
        return { resp::wrongArgs("scan") };

    std::optional<std::string> pattern;
    std::optional<KeyType> typeFilter;
    int64_t count = 10;
    for (size_t i = 2; i < args.size(); i += 2)
    {
        const auto opt = toUpper(args[i]);
        if (opt == "MATCH")
            pattern = args[i + 1];
        else if (opt == "COUNT")
        {
            if (!parseInt64(args[i + 1], count))
                return { resp::notInteger };
            if (count < 1)
                return { resp::syntaxError };
        }
        else if (opt == "TYPE")
        {
            const auto typeStr = toLower(args[i + 1]);
            if (typeStr == "string")
                typeFilter = KeyType::String;
            else if (typeStr == "list")
                typeFilter = KeyType::List;
            else
                // Other Redis types are not supported, so there are no keys of that type
                typeFilter = KeyType::None;
        }
        else
            return { resp::syntaxError };
    }

    std::optional<std::string> startAfter;
    if (args[1] != "0")
    {
        uint64_t cursorId;
        const auto res = std::from_chars(args[1].data(), args[1].data() + args[1].size(), cursorId);
        if (res.ec != std::errc() || res.ptr != args[1].data() + args[1].size())
            return { resp::error("ERR invalid cursor") };

        std::scoped_lock lock(cursorsMutex);
        auto it = cursors.find(cursorId);
        if (it == cursors.end())
            return { resp::error("ERR invalid or expired cursor") };
        startAfter = it->second;
    }

    if (auto err = checkAuth(state, args[0], { Action::ListObjects }, std::nullopt))
        return { *err };
    BucketCtx ctx;
    if (auto err = getBucketCtx(ctx))
        return { *err };

    const auto prefix = pattern ? globPrefix(*pattern) : std::string();

    std::vector<std::string> keys;
    int64_t examined = 0;
    std::string lastKey;
    bool atEnd;
    if (!iterateKeys(ctx.id, startAfter, prefix, [&](const std::string& key, const KeyType type)
                     {
                         if (examined >= count)
                             return false;
                         lastKey = key;
                         ++examined;
                         if (type != KeyType::None && (!typeFilter || type == *typeFilter) && (!pattern || globMatch(*pattern, key)))
                             keys.push_back(key);
                         return true; }, atEnd))
    {
        return { resp::error("ERR error listing keys") };
    }

    std::string cursorStr = "0";
    if (!atEnd)
    {
        uint64_t cursorId;
        do
        {
            // Keep it in the range of doubles (some clients parse it as number)
            cursorId = folly::Random::secureRand64() & ((1ULL << 53) - 1);
        } while (cursorId == 0);

        std::scoped_lock lock(cursorsMutex);
        cursors[cursorId] = lastKey;
        cursorsOrder.push_back(cursorId);
        while (cursorsOrder.size() > maxScanCursors)
        {
            cursors.erase(cursorsOrder.front());
            cursorsOrder.pop_front();
        }
        cursorStr = std::to_string(cursorId);
    }

    std::string reply = resp::arrayHeader(2);
    resp::appendBulk(reply, cursorStr);
    reply += resp::arrayHeader(keys.size());
    for (const auto& key : keys)
        resp::appendBulk(reply, key);
    return { reply };
}

RedisServer::ExecResult RedisServer::cmdKeys(ConnState& state, const std::vector<std::string>& args)
{
    if (args.size() != 2)
        return { resp::wrongArgs("keys") };

    if (auto err = checkAuth(state, args[0], { Action::ListObjects }, std::nullopt))
        return { *err };
    BucketCtx ctx;
    if (auto err = getBucketCtx(ctx))
        return { *err };

    const auto& pattern = args[1];
    std::vector<std::string> keys;
    bool atEnd;
    if (!iterateKeys(ctx.id, std::nullopt, globPrefix(pattern), [&](const std::string& key, const KeyType type)
                     {
                         if (type != KeyType::None && globMatch(pattern, key))
                             keys.push_back(key);
                         return true; }, atEnd))
    {
        return { resp::error("ERR error listing keys") };
    }

    std::string reply = resp::arrayHeader(keys.size());
    for (const auto& key : keys)
        resp::appendBulk(reply, key);
    return { reply };
}

RedisServer::ExecResult RedisServer::cmdDbsize(ConnState& state, const std::vector<std::string>& args)
{
    if (args.size() != 1)
        return { resp::wrongArgs("dbsize") };

    if (auto err = checkAuth(state, args[0], { Action::ListObjects }, std::nullopt))
        return { *err };
    BucketCtx ctx;
    if (auto err = getBucketCtx(ctx))
        return { *err };

    int64_t count = 0;
    bool atEnd;
    if (!iterateKeys(ctx.id, std::nullopt, std::string(), [&](const std::string& /*key*/, const KeyType type)
                     {
                         if (type != KeyType::None)
                             ++count;
                         return true; }, atEnd))
    {
        return { resp::error("ERR error listing keys") };
    }

    return { resp::integer(count) };
}

RedisServer::ExecResult RedisServer::cmdFlushdb(ConnState& state, const std::vector<std::string>& args)
{
    if (args.size() > 2)
        return { resp::wrongArgs(toLower(args[0])) };
    if (args.size() == 2)
    {
        const auto opt = toUpper(args[1]);
        if (opt != "SYNC" && opt != "ASYNC")
            return { resp::syntaxError };
    }

    if (auto err = checkAuth(state, args[0], { Action::ListObjects }, std::nullopt))
        return { *err };
    if (auto err = checkAuth(state, args[0], { Action::DeleteObject }, "*"))
        return { *err };
    BucketCtx ctx;
    if (auto err = getBucketCtx(ctx))
        return { *err };

    while (true)
    {
        // Collect a batch of entries (all versions) and delete them
        std::vector<std::string> batch;
        {
            SingleFileStorage::ListIterData iterData;
            SCOPE_EXIT { sfs.list_iter_stop(iterData); };

            const auto bucketId = ctx.id;
            const auto start = make_key(std::string(), bucketId, std::numeric_limits<int64_t>::max());
            if (!sfs.list_iter_start(start, start, [bucketId](const std::string& iterKey)
                                     { return extractKeyInfoView(iterKey).bucketId == bucketId; }, iterData))
            {
                return { resp::error("ERR error listing keys") };
            }

            while (batch.size() < flushBatchSize)
            {
                std::string keyBin, md5sum;
                int64_t offset, size, lastModified;
                std::vector<SingleFileStorage::SPunchItem> extraExts;
                if (!sfs.iter_curr_val(keyBin, offset, size, extraExts, lastModified, md5sum, iterData.curr))
                    break;

                if (extractKeyInfoView(keyBin).bucketId != bucketId)
                    break;

                batch.push_back(std::move(keyBin));

                if (!sfs.list_iter_next(iterData))
                    return { resp::error("ERR error listing keys") };
            }
        }

        if (batch.empty())
            break;

        for (const auto& keyBin : batch)
        {
            const auto rc = sfs.del(keyBin, SingleFileStorage::DelAction::Del, false);
            if (rc != 0)
                return { resp::error(fmt::format("ERR storage error {}", rc)) };
        }

        if (!commit())
            return { resp::error("ERR storage commit failed") };
    }

    return { resp::ok };
}

RedisServer::RedisServer(SingleFileStorage& sfs, const std::string& bucket)
    : sfs(sfs), bucketName(bucket)
{
}

RedisServer::~RedisServer()
{
    stop();
}

bool RedisServer::initBucket(const std::string& bucket)
{
    if (!buckets::isValidBucketName(bucket))
    {
        XLOGF(ERR, "Invalid Redis bucket name {}", bucket);
        return false;
    }

    if (const auto bucketId = buckets::getBucket(bucket))
    {
        const auto bucketInfo = buckets::getBucketInfo(bucket);
        if (bucketInfo && bucketInfo->versioning != buckets::VersioningState::Enabled)
            XLOGF(WARN, "Versioning is not enabled on Redis bucket {}. Redis commands will fail until it is enabled", bucket);

        if (bucketInfo && !bucketInfo->redis)
            XLOGF(WARN, "Marking existing bucket {} as Redis bucket. List elements already in it are not counted", bucket);

        if (!buckets::setRedis(*bucketId))
        {
            XLOGF(ERR, "Error marking bucket {} as Redis bucket", bucket);
            return false;
        }
        return true;
    }

    const auto bucketId = buckets::addBucket(bucket, false);
    if (bucketId < 0)
    {
        XLOGF(ERR, "Error creating Redis bucket {}", bucket);
        return false;
    }

    if (buckets::setVersioning(bucket, buckets::VersioningState::Enabled) != buckets::SetVersioningResult::Ok)
    {
        XLOGF(ERR, "Error enabling versioning on Redis bucket {}", bucket);
        return false;
    }

    if (!buckets::setRedis(bucketId))
    {
        XLOGF(ERR, "Error marking bucket {} as Redis bucket", bucket);
        return false;
    }

    XLOGF(INFO, "Created Redis bucket {} with versioning enabled", bucket);
    return true;
}
