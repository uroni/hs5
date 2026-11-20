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
#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
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
#include <folly/lang/Bits.h>
#include <folly/logging/xlog.h>
#include <gflags/gflags.h>
#include <lauxlib.h>
#include <limits>
#include <lua.h>
#include <lualib.h>
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
DEFINE_int32(test_inline_read_delay_ms, 0, "Delay inline GETs (get_in_worker_threshold) between looking up and reading the value. For testing only");
DEFINE_int64(redis_max_value_size, 512 * 1024 * 1024, "Max size of values written or read via the Redis protocol (larger objects can only be accessed via S3)");
DEFINE_int64(redis_collection_inline_max_entries, 128,
             "Hashes, sets and sorted sets with at most this many elements are stored inline (in the metadata of the key). Larger ones "
             "are stored with one entry per element");
DEFINE_int64(redis_collection_inline_max_bytes, 8192, "Max serialized size of hashes, sets and sorted sets stored inline");
DEFINE_int32(redis_script_timeout_ms, 5000, "Lua scripts (EVAL) running longer than this are aborted");
DEFINE_bool(redis_script_always_global, false,
            "Run all Lua scripts with the global lock held exclusively (no other command runs concurrently, like in a "
            "single-threaded Redis server). Otherwise scripts only lock the keys they declare, unless they access undeclared keys");
DEFINE_bool(redis_script_allow_undeclared_keys, false,
            "Allow Lua scripts to access keys they did not declare after modifying something (without atomicity). Otherwise such "
            "scripts fail. Scripts accessing undeclared keys before modifying anything are run again with a global lock");

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
    // Pub/sub messages not yet sent to a client. If more, the client is disconnected
    constexpr size_t maxPubsubBuffer = 32 * 1024 * 1024;

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

    using KeyType = RedisServer::KeyType;

    // Content types of hashes, sets and sorted sets (stored as one serialized object). Registered content types (with
    // id), like the list content type
    constexpr std::string_view hashContentTypeStr = "application/cdmi-object";
    constexpr std::string_view setContentTypeStr = "application/cdmi-container";
    constexpr std::string_view zsetContentTypeStr = "application/cdmi-domain";

    const ContentType& collectionContentType(const KeyType type)
    {
        static const ContentType hash = contentTypeFromStr(hashContentTypeStr);
        static const ContentType set = contentTypeFromStr(setContentTypeStr);
        static const ContentType zset = contentTypeFromStr(zsetContentTypeStr);
        assert(hash.contentType != 0 && set.contentType != 0 && zset.contentType != 0);
        switch (type)
        {
            case KeyType::Hash:
                return hash;
            case KeyType::Set:
                return set;
            default:
                assert(type == KeyType::ZSet);
                return zset;
        }
    }

    KeyType keyTypeFromContentType(const ContentType& contentType)
    {
        const auto id = contentType.contentType;
        if (id == 0)
            return KeyType::String;
        if (id == listContentType().contentType)
            return KeyType::List;
        if (id == collectionContentType(KeyType::Hash).contentType)
            return KeyType::Hash;
        if (id == collectionContentType(KeyType::Set).contentType)
            return KeyType::Set;
        if (id == collectionContentType(KeyType::ZSet).contentType)
            return KeyType::ZSet;
        return KeyType::String;
    }

    // Type and expiry of a key from the metadata of its newest version
    std::pair<KeyType, std::optional<RedisServer::ExpiryInfo>> keyInfoFromMetadata(const std::string& md5sum);

    KeyType readerKeyType(ObjectReader& reader)
    {
        const auto objMetadata = reader.GetObjMetadata();
        if (objMetadata == nullptr)
            return KeyType::String;
        return keyTypeFromContentType(objMetadata->contentType);
    }

    const char* keyTypeName(const KeyType type)
    {
        switch (type)
        {
            case KeyType::None:
                return "none";
            case KeyType::String:
                return "string";
            case KeyType::List:
                return "list";
            case KeyType::Hash:
                return "hash";
            case KeyType::Set:
                return "set";
            case KeyType::ZSet:
                return "zset";
        }
        return "none";
    }

    int64_t nowMs()
    {
        return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    }

    // Key expiry is stored in the object metadata (ObjMetadata::LockInfo) of the value. Lists store it after the count in
    // their counter (varint expiry time, varint index version), which is deleted with the last element.
    // Additionally there is an index ordered by expiry time in the counters bucket (see buckets::getCountersBucket),
    // which also has the list counters (Redis key, version 0): Key 0xff 'i' + big endian expiry time, with a unique
    // version > 0 and the Redis key as md5sum. Byte 0xff is reserved (not allowed as first byte of Redis keys), and
    // the version is never 0, so they cannot overlap with counters
    const std::string reservedKeyPrefix("\xff", 1);
    const std::string expiryIndexPrefix("\xffi", 2);

    std::optional<RedisServer::ExpiryInfo> expiryFromObjMetadata(const ObjMetadata* objMetadata)
    {
        if (objMetadata == nullptr || !objMetadata->lockInfo || !objMetadata->lockInfo->redisExpiry())
            return std::nullopt;
        return RedisServer::ExpiryInfo{ .ms = objMetadata->lockInfo->redisExpiryMs, .indexVersion = objMetadata->lockInfo->redisExpiryIndexVersion };
    }

    std::optional<RedisServer::ExpiryInfo> expiryFromMetadata(const std::string& md5sum)
    {
        if (isTombstone(md5sum))
            return std::nullopt;
        const auto objMetadata = S3Handler::objMetadataFromMetadata(md5sum);
        return expiryFromObjMetadata(objMetadata.get());
    }

    bool isExpired(const std::optional<RedisServer::ExpiryInfo>& expiry)
    {
        return expiry && expiry->ms <= nowMs();
    }

    std::pair<KeyType, std::optional<RedisServer::ExpiryInfo>> keyInfoFromMetadata(const std::string& md5sum)
    {
        if (isTombstone(md5sum))
            return { KeyType::None, std::nullopt };
        const auto objMetadata = S3Handler::objMetadataFromMetadata(md5sum);
        if (!objMetadata)
            return { KeyType::String, std::nullopt };
        return { keyTypeFromContentType(objMetadata->contentType), expiryFromObjMetadata(objMetadata.get()) };
    }

    std::string expiryIndexKey(const int64_t expiryMs)
    {
        const auto val = folly::Endian::big(static_cast<uint64_t>(expiryMs));
        std::string ret = expiryIndexPrefix;
        ret.append(reinterpret_cast<const char*>(&val), sizeof(val));
        return ret;
    }

    std::optional<int64_t> parseExpiryIndexKey(const std::string_view key)
    {
        uint64_t val;
        if (key.size() != expiryIndexPrefix.size() + sizeof(val) || !key.starts_with(expiryIndexPrefix))
            return std::nullopt;
        std::memcpy(&val, key.data() + expiryIndexPrefix.size(), sizeof(val));
        return static_cast<int64_t>(folly::Endian::big(val));
    }

    // Parses a double like Redis (also inf, -inf, +inf). Rejects NaN
    bool parseDouble(const std::string_view str, double& val)
    {
        if (str.empty() || str.size() > 256 || std::isspace(static_cast<unsigned char>(str[0])))
            return false;
        const auto lower = toLower(str);
        if (lower == "inf" || lower == "+inf" || lower == "infinity" || lower == "+infinity")
        {
            val = std::numeric_limits<double>::infinity();
            return true;
        }
        if (lower == "-inf" || lower == "-infinity")
        {
            val = -std::numeric_limits<double>::infinity();
            return true;
        }
        const std::string tmp(str);
        char* end = nullptr;
        errno = 0;
        val = std::strtod(tmp.c_str(), &end);
        if (end != tmp.c_str() + tmp.size() || std::isnan(val) || errno == ERANGE)
            return false;
        return true;
    }

    std::string formatDouble(const double val)
    {
        if (std::isinf(val))
            return val > 0 ? "inf" : "-inf";
        return fmt::format("{}", val);
    }

    // Score range bound of ZRANGEBYSCORE etc. ("(1.5" is exclusive)
    struct ScoreBound
    {
        double val;
        bool exclusive;
    };

    bool parseScoreBound(const std::string_view str, ScoreBound& bound)
    {
        bound.exclusive = !str.empty() && str[0] == '(';
        return parseDouble(bound.exclusive ? str.substr(1) : str, bound.val);
    }

    bool aboveMin(const double score, const ScoreBound& min)
    {
        return min.exclusive ? score > min.val : score >= min.val;
    }

    bool belowMax(const double score, const ScoreBound& max)
    {
        return max.exclusive ? score < max.val : score <= max.val;
    }

    // Context of a transaction (EXEC) or script running on this thread. Key locks are held by the transaction, so
    // commands executed by it don't lock keys. Commits are done at the end
    struct TxContext
    {
        bool needCommit = false;
        bool inScript = false;
        // Modified entries of large collections (which listings only see after committing)
        bool largeWrites = false;
    };

    thread_local TxContext* currentTx = nullptr;

    // This thread holds the global lock exclusively (no other command runs concurrently)
    thread_local bool inGlobalLock = false;

    // Positions of keys in the arguments of a command
    struct KeySpec
    {
        // Index of the first key. 0 if the command has no keys
        int first = 0;
        // Index of the last key. Negative: relative to the end (-1 is the last argument)
        int last = 0;
        int step = 1;
    };

    const std::unordered_map<std::string_view, KeySpec>& keySpecs()
    {
        static const std::unordered_map<std::string_view, KeySpec> specs = []()
        {
            std::unordered_map<std::string_view, KeySpec> ret;
            for (const auto name : { "GET", "SET", "SETNX", "SETEX", "PSETEX", "STRLEN", "INCR", "DECR", "INCRBY", "DECRBY", "TYPE",
                                     "LPUSH", "RPUSH", "LPUSHX", "RPUSHX", "LPOP", "RPOP", "LLEN", "LRANGE", "LINDEX", "GETDEL",
                                     "EXPIRE", "PEXPIRE", "EXPIREAT", "PEXPIREAT", "TTL", "PTTL", "PERSIST", "EXPIRETIME", "PEXPIRETIME",
                                     "HSET", "HMSET", "HSETNX", "HGET", "HMGET", "HDEL", "HGETALL", "HLEN", "HEXISTS", "HKEYS", "HVALS",
                                     "HINCRBY", "HSTRLEN", "SADD", "SREM", "SMEMBERS", "SISMEMBER", "SMISMEMBER", "SCARD", "ZADD", "ZREM",
                                     "ZCARD", "ZSCORE", "ZMSCORE", "ZCOUNT", "ZINCRBY", "ZRANGE", "ZREVRANGE", "ZRANGEBYSCORE",
                                     "ZREVRANGEBYSCORE", "ZREMRANGEBYSCORE", "ZREMRANGEBYRANK", "ZRANK", "ZREVRANK" })
                ret[name] = KeySpec{ .first = 1, .last = 1, .step = 1 };
            for (const auto name : { "MGET", "DEL", "UNLINK", "EXISTS", "WATCH" })
                ret[name] = KeySpec{ .first = 1, .last = -1, .step = 1 };
            ret["MSET"] = KeySpec{ .first = 1, .last = -1, .step = 2 };
            ret["BLPOP"] = KeySpec{ .first = 1, .last = -2, .step = 1 };
            ret["BRPOP"] = KeySpec{ .first = 1, .last = -2, .step = 1 };
            return ret;
        }();
        return specs;
    }

    // Commands without keys
    const std::unordered_set<std::string_view>& keylessCommands()
    {
        static const std::unordered_set<std::string_view> cmds = { "PING", "ECHO", "QUIT", "AUTH", "HELLO", "SELECT", "CLIENT", "COMMAND",
                                                                   "INFO", "RESET", "SCAN", "KEYS", "DBSIZE", "FLUSHDB", "FLUSHALL",
                                                                   "PUBLISH", "SUBSCRIBE", "UNSUBSCRIBE", "PSUBSCRIBE", "PUNSUBSCRIBE",
                                                                   "MULTI", "EXEC", "DISCARD", "UNWATCH", "SCRIPT", "TIME", "EVAL",
                                                                   "EVALSHA", "EVAL_RO", "EVALSHA_RO" };
        return cmds;
    }

    bool isKnownCommand(const std::string& cmd)
    {
        return keySpecs().contains(cmd) || keylessCommands().contains(cmd);
    }

    // Keys accessed by a command (upper case name cmd). EVAL/EVALSHA: the declared keys
    std::vector<std::string> commandKeys(const std::string& cmd, const std::vector<std::string>& args)
    {
        std::vector<std::string> keys;
        if (cmd == "EVAL" || cmd == "EVALSHA" || cmd == "EVAL_RO" || cmd == "EVALSHA_RO")
        {
            int64_t numKeys;
            if (args.size() < 3 || !parseInt64(args[2], numKeys) || numKeys < 0 || static_cast<size_t>(numKeys) > args.size() - 3)
                return keys;
            keys.assign(args.begin() + 3, args.begin() + 3 + numKeys);
            return keys;
        }

        const auto it = keySpecs().find(cmd);
        if (it == keySpecs().end())
            return keys;
        const auto& spec = it->second;
        const auto argc = static_cast<int>(args.size());
        const int last = spec.last < 0 ? argc + spec.last : spec.last;
        for (int i = spec.first; i <= last && i < argc; i += spec.step)
            keys.push_back(args[static_cast<size_t>(i)]);
        return keys;
    }

    // Converts a RESP2 reply to a Lua value (pushed on the stack). Returns the position after the reply
    size_t pushRespToLua(lua_State* L, const std::string& reply, size_t pos, bool& isError, std::string& errorMsg, const int depth = 0);

    std::string readRespLine(const std::string& reply, size_t& pos)
    {
        const auto end = reply.find("\r\n", pos);
        if (end == std::string::npos)
        {
            pos = reply.size();
            return std::string();
        }
        auto ret = reply.substr(pos, end - pos);
        pos = end + 2;
        return ret;
    }

    size_t pushRespToLua(lua_State* L, const std::string& reply, size_t pos, bool& isError, std::string& errorMsg, const int depth)
    {
        if (pos >= reply.size())
        {
            lua_pushboolean(L, 0);
            return pos;
        }
        luaL_checkstack(L, 4, "reply too deep");
        const char type = reply[pos++];
        auto line = readRespLine(reply, pos);
        switch (type)
        {
            case '+':
                lua_newtable(L);
                lua_pushlstring(L, line.data(), line.size());
                lua_setfield(L, -2, "ok");
                return pos;
            case '-':
                if (depth == 0)
                {
                    isError = true;
                    errorMsg = line;
                }
                lua_newtable(L);
                lua_pushlstring(L, line.data(), line.size());
                lua_setfield(L, -2, "err");
                return pos;
            case ':':
            {
                int64_t val = 0;
                parseInt64(line, val);
                lua_pushinteger(L, val);
                return pos;
            }
            case '$':
            {
                int64_t len = -1;
                parseInt64(line, len);
                if (len < 0)
                {
                    lua_pushboolean(L, 0);
                    return pos;
                }
                lua_pushlstring(L, reply.data() + pos, static_cast<size_t>(len));
                return pos + static_cast<size_t>(len) + 2;
            }
            case '*':
            {
                int64_t len = -1;
                parseInt64(line, len);
                if (len < 0)
                {
                    lua_pushboolean(L, 0);
                    return pos;
                }
                lua_createtable(L, static_cast<int>(std::min(len, static_cast<int64_t>(1024))), 0);
                for (int64_t i = 0; i < len; ++i)
                {
                    bool elemError = false;
                    std::string elemErrorMsg;
                    pos = pushRespToLua(L, reply, pos, elemError, elemErrorMsg, depth + 1);
                    lua_rawseti(L, -2, i + 1);
                }
                return pos;
            }
            default:
                lua_pushboolean(L, 0);
                return reply.size();
        }
    }

    // Converts the Lua value at the top of the stack to a reply
    void luaToResp(lua_State* L, std::string& out, const RedisServer::ConnState& state, const int depth = 0)
    {
        switch (lua_type(L, -1))
        {
            case LUA_TSTRING:
            {
                size_t len;
                const char* str = lua_tolstring(L, -1, &len);
                resp::appendBulk(out, std::string_view(str, len));
                break;
            }
            case LUA_TBOOLEAN:
                if (lua_toboolean(L, -1))
                    out += resp::integer(1);
                else
                    out += resp::nullBulk(state);
                break;
            case LUA_TNUMBER:
                if (lua_isinteger(L, -1))
                    out += resp::integer(lua_tointeger(L, -1));
                else
                    out += resp::integer(static_cast<int64_t>(lua_tonumber(L, -1)));
                break;
            case LUA_TTABLE:
            {
                if (depth > 100)
                {
                    out += resp::error("ERR reached lua stack limit");
                    break;
                }
                luaL_checkstack(L, 4, "reply too deep");
                lua_getfield(L, -1, "err");
                if (lua_type(L, -1) == LUA_TSTRING)
                {
                    std::string err = lua_tostring(L, -1);
                    lua_pop(L, 1);
                    // Error replies have to start with an error code
                    if (err.find(' ') == std::string::npos || !std::isupper(static_cast<unsigned char>(err[0])))
                        err = "ERR " + err;
                    std::replace(err.begin(), err.end(), '\r', ' ');
                    std::replace(err.begin(), err.end(), '\n', ' ');
                    out += resp::error(err);
                    break;
                }
                lua_pop(L, 1);
                lua_getfield(L, -1, "ok");
                if (lua_type(L, -1) == LUA_TSTRING)
                {
                    std::string ok = lua_tostring(L, -1);
                    lua_pop(L, 1);
                    std::replace(ok.begin(), ok.end(), '\r', ' ');
                    std::replace(ok.begin(), ok.end(), '\n', ' ');
                    out += resp::simple(ok);
                    break;
                }
                lua_pop(L, 1);

                // Array up to the first nil
                std::string elems;
                size_t n = 0;
                while (true)
                {
                    lua_rawgeti(L, -1, static_cast<lua_Integer>(n + 1));
                    if (lua_isnil(L, -1))
                    {
                        lua_pop(L, 1);
                        break;
                    }
                    luaToResp(L, elems, state, depth + 1);
                    lua_pop(L, 1);
                    ++n;
                }
                out += resp::arrayHeader(n);
                out += elems;
                break;
            }
            default:
                out += resp::nullBulk(state);
                break;
        }
    }

    std::string sha1Hex(const std::string_view data)
    {
        unsigned char md[EVP_MAX_MD_SIZE];
        unsigned int mdLen = 0;
        if (EVP_Digest(data.data(), data.size(), md, &mdLen, EVP_sha1(), nullptr) != 1)
            return std::string();
        std::string ret;
        for (unsigned int i = 0; i < mdLen; ++i)
            ret += fmt::format("{:02x}", md[i]);
        return ret;
    }
} // namespace

namespace
{
    // Fields/members longer than this are stored with their SHA-256 in the key of large collections (LMDB key size limit)
    constexpr size_t maxFieldKeyLen = 256;

    char collectionTag(const KeyType type)
    {
        switch (type)
        {
            case KeyType::Hash:
                return 'h';
            case KeyType::Set:
                return 's';
            default:
                return 'z';
        }
    }

    std::string encodeKey(const std::string_view key)
    {
        CWData data;
        data.addVarInt(static_cast<int64_t>(key.size()));
        data.addBuffer(key.data(), key.size());
        return std::string(data.getDataPtr(), data.getDataSize());
    }

    // Prefix of the entries of a large collection (in the counters bucket)
    std::string collectionPrefix(const KeyType type, const std::string_view key)
    {
        std::string ret = reservedKeyPrefix;
        ret.push_back(collectionTag(type));
        ret += encodeKey(key);
        return ret;
    }

    // Key of the counter of the entries of a large collection (maintained by the storage engine)
    std::string collectionCounterKey(const std::string_view key)
    {
        std::string ret = reservedKeyPrefix;
        ret.push_back('c');
        ret += encodeKey(key);
        return ret;
    }

    bool isLongField(const std::string_view field)
    {
        return field.size() > maxFieldKeyLen;
    }

    std::string sha256(const std::string_view data)
    {
        unsigned char md[EVP_MAX_MD_SIZE];
        unsigned int mdLen = 0;
        if (EVP_Digest(data.data(), data.size(), md, &mdLen, EVP_sha256(), nullptr) != 1)
            return std::string();
        return std::string(reinterpret_cast<const char*>(md), mdLen);
    }

    // Field in an entry key: 0 + field, or 1 + SHA-256 of long fields (the field is then stored in the entry)
    std::string encodeField(const std::string_view field)
    {
        std::string ret;
        if (isLongField(field))
        {
            ret.push_back('\1');
            ret += sha256(field);
        }
        else
        {
            ret.push_back('\0');
            ret.append(field);
        }
        return ret;
    }

    // Big endian double whose byte order is the numeric order
    std::string sortableScore(double score)
    {
        if (score == 0)
            score = 0;
        uint64_t bits;
        std::memcpy(&bits, &score, sizeof(bits));
        if (bits >> 63)
            bits = ~bits;
        else
            bits |= 1ULL << 63;
        bits = folly::Endian::big(bits);
        return std::string(reinterpret_cast<const char*>(&bits), sizeof(bits));
    }

    double scoreFromSortable(const std::string_view data)
    {
        uint64_t bits;
        std::memcpy(&bits, data.data(), sizeof(bits));
        bits = folly::Endian::big(bits);
        if (bits >> 63)
            bits &= ~(1ULL << 63);
        else
            bits = ~bits;
        double score;
        std::memcpy(&score, &bits, sizeof(score));
        return score;
    }

    // Counter key of an entry of a large collection (hash/set entries, sorted set member entries), or empty
    std::string collectionCounterKeyOfEntry(const std::string_view entryKey)
    {
        if (entryKey.size() < 3 || !entryKey.starts_with(reservedKeyPrefix))
            return std::string();
        const char tag = entryKey[1];
        if (tag != 'h' && tag != 's' && tag != 'z')
            return std::string();
        CRData data(entryKey.data() + 2, entryKey.size() - 2);
        int64_t keyLen;
        if (!data.getVarInt(&keyLen) || keyLen < 0 || static_cast<int64_t>(data.getLeft()) < keyLen + 1)
            return std::string();
        const auto encodedLen = static_cast<size_t>(data.getStreampos()) + static_cast<size_t>(keyLen);
        // Sorted set: Only member entries ('m') are counted, not the score index ('s')
        if (tag == 'z' && entryKey[2 + encodedLen] != 'm')
            return std::string();
        std::string ret = reservedKeyPrefix;
        ret.push_back('c');
        ret.append(entryKey.substr(2, encodedLen));
        return ret;
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

    // Appends a complete reply
    void appendResult(ExecResult res)
    {
        curr.append(res.reply);
        for (auto& part : res.streamParts)
        {
            curr.append(part.prefix);
            parts.push_back(StreamPart{ .prefix = std::move(curr), .reader = std::move(part.reader) });
            curr = std::string();
        }
        curr.append(res.tail);
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
        closedFlag = true;
        if (hadSubscriptions)
            server.unsubscribeAll(this);
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

    // Sends a pub/sub message (any thread). Closes the connection if the client does not read fast enough
    static void push(std::shared_ptr<RedisConnection> conn, std::string msg)
    {
        auto evb = conn->evb;
        evb->runInEventBaseThread([conn = std::move(conn), msg = std::move(msg)]() mutable
                                  {
            conn->assertOnEvb();
            if (conn->closed)
                return;
            if (conn->sock && conn->sock->getRawBytesBuffered() > maxPubsubBuffer)
            {
                XLOGF(INFO, "Redis connection {} does not read pub/sub messages fast enough. Closing it", conn->connId());
                conn->close();
                return;
            }
            conn->replies.push_back(RedisServer::ExecResult{ .reply = std::move(msg) });
            conn->sendReplies(); });
    }

    bool isClosed() const
    {
        return closedFlag;
    }

    void markSubscribed()
    {
        hadSubscriptions = true;
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
    // closed, for other threads
    std::atomic<bool> closedFlag{ false };
    // Connection subscribed to pub/sub channels at some point (has to be unsubscribed when closing)
    std::atomic<bool> hadSubscriptions{ false };
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

size_t RedisServer::keyLockIdx(const std::string& key) const
{
    return std::hash<std::string>{}(key) % keyLocks.size();
}

std::shared_mutex& RedisServer::keyLock(const std::string& key)
{
    return keyLocks[keyLockIdx(key)];
}

void RedisServer::touchKeyLock(const size_t idx)
{
    keyEpochs[idx].fetch_add(1, std::memory_order_relaxed);
}

// Exclusively locks the key lock of a key (for modifying it). Does nothing in a transaction/script (which holds the
// locks of its keys)
class RedisServer::KeyLock
{
public:
    KeyLock(RedisServer& server, const std::string& key) NO_THREAD_SAFETY_ANALYSIS
    {
        if (currentTx != nullptr)
            return;
        const auto idx = server.keyLockIdx(key);
        lock = &server.keyLocks[idx];
        lock->lock();
        server.touchKeyLock(idx);
    }

    ~KeyLock() NO_THREAD_SAFETY_ANALYSIS
    {
        if (lock != nullptr)
            lock->unlock();
    }

    KeyLock(const KeyLock&) = delete;
    KeyLock& operator=(const KeyLock&) = delete;

private:
    std::shared_mutex* lock = nullptr;
};

// Locks the key locks of multiple keys, so multi-key commands are atomic. Locks in index order (each lock once), so
// it cannot deadlock with other multi-key commands. Single key commands only hold one lock at a time. Does nothing
// in a transaction/script, unless force is set
class RedisServer::MultiKeyLock
{
public:
    MultiKeyLock(RedisServer& server, const std::vector<std::string>& args, const size_t start, const size_t step, const bool exclusive)
        : exclusive(exclusive)
    {
        if (currentTx != nullptr)
            return;
        std::vector<size_t> idxs;
        for (size_t i = start; i < args.size(); i += step)
            idxs.push_back(server.keyLockIdx(args[i]));
        lockIdxs(server, std::move(idxs), exclusive);
    }

    // Locks the keys exclusively (without incrementing their modification epochs)
    MultiKeyLock(RedisServer& server, const std::vector<std::string>& keys)
        : exclusive(true)
    {
        std::vector<size_t> idxs;
        for (const auto& key : keys)
            idxs.push_back(server.keyLockIdx(key));
        lockIdxs(server, std::move(idxs), false);
    }

    ~MultiKeyLock() NO_THREAD_SAFETY_ANALYSIS
    {
        for (auto it = locks.rbegin(); it != locks.rend(); ++it)
        {
            if (exclusive)
                server->keyLocks[*it].unlock();
            else
                server->keyLocks[*it].unlock_shared();
        }
    }

    // Increments the modification epochs of the locked keys
    void touch()
    {
        for (const auto idx : locks)
            server->touchKeyLock(idx);
    }

    MultiKeyLock(const MultiKeyLock&) = delete;
    MultiKeyLock& operator=(const MultiKeyLock&) = delete;

private:
    void lockIdxs(RedisServer& srv, std::vector<size_t> idxs, const bool touchEpochs) NO_THREAD_SAFETY_ANALYSIS
    {
        server = &srv;
        std::sort(idxs.begin(), idxs.end());
        idxs.erase(std::unique(idxs.begin(), idxs.end()), idxs.end());
        locks = std::move(idxs);
        for (const auto idx : locks)
        {
            if (exclusive)
                srv.keyLocks[idx].lock();
            else
                srv.keyLocks[idx].lock_shared();
        }
        if (touchEpochs)
            touch();
    }

    const bool exclusive;
    RedisServer* server = nullptr;
    std::vector<size_t> locks;
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
    {
        XLOGF(INFO, "Redis server listening on {}:{} (bucket {})", ip, port, bucketName);
        // Large collections exist if there are collection counters
        if (const auto bucketInfo = buckets::getBucketInfo(bucketName))
            anyLargeCollections = hasEntries(buckets::getCountersBucket(bucketInfo->id), reservedKeyPrefix + "c");
        expiryThread = std::thread([this]()
                                   { expiryThreadFunc(); });
    }

    return ok;
}

void RedisServer::stop()
{
    if (!ioPool)
        return;

    if (expiryThread.joinable())
    {
        {
            std::scoped_lock lock(expiryThreadMutex);
            expiryThreadStop = true;
        }
        expiryThreadCond.notify_all();
        expiryThread.join();
    }

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
    if (key.starts_with(reservedKeyPrefix))
        return resp::error("ERR keys starting with byte 0xff are reserved");
    return std::nullopt;
}

bool RedisServer::listEntries(const int64_t bucketId, const std::string& key, std::vector<Entry>& entries, const size_t maxElements)
{
    syncListing();

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
        const auto [type, expiry] = keyInfoFromMetadata(md5sum);
        entries.push_back(Entry{ .version = keyInfo.version, .tombstone = tombstone, .isList = type == KeyType::List, .type = type, .expiry = expiry });
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

    const auto [type, expiry] = keyInfoFromMetadata(res.md5sum);
    return Entry{ .version = extractKeyInfoView(res.key).version, .tombstone = isTombstone(res.md5sum), .isList = type == KeyType::List, .type = type, .expiry = expiry };
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
        const auto expiry = expiryFromObjMetadata(reader->GetObjMetadata());
        if (isExpired(expiry))
            return ReadRes{ .status = ReadRes::Status::NotFound };
        if (readerKeyType(*reader) != KeyType::String)
            return ReadRes{ .status = ReadRes::Status::WrongType };

        auto ret = readFromReader(std::move(reader), key, mode);
        ret.expiry = expiry;
        return ret;
    }
    catch (const ObjectReadError& e)
    {
        if (e.notFound())
            return ReadRes{ .status = ReadRes::Status::NotFound };

        XLOGF(WARN, "Error reading Redis key {}: {}", key, e.what());
        return ReadRes{ .status = ReadRes::Status::Error, .value = e.what() };
    }
}

int RedisServer::prepareValue(const int64_t bucketId, const std::string& key, const ValueArg& value, const ContentType& contentType,
                              const std::optional<ExpiryInfo>& expiry, PreparedValue& prepared, const std::string* redisData)
{
    ObjMetadata objMetadata;
    objMetadata.contentType = contentType;
    if (redisData != nullptr)
        objMetadata.redisData = *redisData;
    if (expiry)
    {
        objMetadata.lockInfo.emplace();
        objMetadata.lockInfo->flags = lockinfo_flag_redis_expiry;
        objMetadata.lockInfo->redisExpiryMs = expiry->ms;
        objMetadata.lockInfo->redisExpiryIndexVersion = expiry->indexVersion;
    }

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

int RedisServer::setValue(const int64_t bucketId, const std::string& key, const ValueArg& value, const std::optional<ExpiryInfo>& expiry)
{
    return setValue(bucketId, key, value, stringContentType(), expiry);
}

int RedisServer::setValue(const int64_t bucketId, const std::string& key, const ValueArg& value, const ContentType& contentType,
                          const std::optional<ExpiryInfo>& expiry, const std::string* redisData)
{
    // Strings replace large collections. Delete their entries
    if (contentType.contentType == stringContentType().contentType && anyLargeCollections.load(std::memory_order_relaxed))
    {
        int err = 0;
        const auto entry = findEnd(bucketId, key, true, err);
        if (err != 0)
            return err;
        if (entry && !entry->tombstone && (entry->type == KeyType::Hash || entry->type == KeyType::Set || entry->type == KeyType::ZSet))
        {
            const auto rc = deleteCollectionEntries(bucketId, key);
            if (rc != 0)
                return rc;
        }
    }

    PreparedValue prepared(sfs);
    auto rc = prepareValue(bucketId, key, value, contentType, expiry, prepared, redisData);
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
    // Also the number of elements of large collections (entries in the counters bucket)
    options.counter_key_callback = [](const std::string& fn) -> std::string
    {
        const auto keyInfo = extractKeyInfoView(fn);
        if (buckets::isCountersBucket(keyInfo.bucketId))
        {
            if (!keyInfo.key.starts_with(reservedKeyPrefix) || !buckets::isRedisBucket(keyInfo.bucketId - 3))
                return std::string();
            const auto counterKey = collectionCounterKeyOfEntry(keyInfo.key);
            if (counterKey.empty())
                return std::string();
            return make_key(counterKey, keyInfo.bucketId, 0);
        }
        if (!buckets::isRedisBucket(keyInfo.bucketId))
            return std::string();
        return make_key(keyInfo.key, buckets::getCountersBucket(keyInfo.bucketId), 0);
    };
    options.counter_counts_callback = [](const std::string& fn, const std::string& md5sum)
    {
        // Collection entries (counter_key_callback only returns a counter for them in the counters bucket)
        if (buckets::isCountersBucket(extractKeyInfoView(fn).bucketId))
            return true;
        return isListElement(md5sum);
    };
}

void RedisServer::syncListing()
{
    // Listings without WAL/manual commit only see committed modifications. Commit the modifications of the
    // transaction/script so far (atomicity is ensured by the locks). With WAL/manual commit, listings include
    // uncommitted modifications
    if (currentTx != nullptr && currentTx->needCommit && !sfs.listings_include_uncommitted())
    {
        if (S3Handler::commitStorage(sfs))
            currentTx->needCommit = false;
    }
}

bool RedisServer::commit()
{
    if (currentTx != nullptr)
    {
        // Committed at the end of the transaction/script
        currentTx->needCommit = true;
        return true;
    }
    return S3Handler::commitStorage(sfs);
}

template <typename F>
bool RedisServer::iterateKeys(const int64_t bucketId, const std::optional<std::string>& startAfter, const std::string& prefix, F f, bool& atEnd)
{
    syncListing();

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
            auto [type, expiry] = keyInfoFromMetadata(md5sum);
            if (type == KeyType::List && anyExpiries.load(std::memory_order_relaxed))
            {
                int err = 0;
                expiry = listExpiry(bucketId, *prevKey, err);
            }
            if (isExpired(expiry))
                type = KeyType::None;
            if (!f(*prevKey, type))
                return true;
        }

        if (!sfs.list_iter_next(iterData))
            return false;
    }
}

int RedisServer::deleteKey(const int64_t bucketId, const std::string& key, bool& existed)
{
    existed = false;
    std::vector<Entry> entries;
    if (!listEntries(bucketId, key, entries))
        return EIO;

    // Expiry of lists (in the counter, which is deleted with the last element)
    const auto newest = std::find_if(entries.begin(), entries.end(), [](const Entry& entry)
                                     { return !entry.tombstone; });
    if (newest != entries.end() && newest->isList && anyExpiries.load(std::memory_order_relaxed))
    {
        int err = 0;
        const auto expiry = listExpiry(bucketId, key, err);
        if (err != 0)
            return err;
        if (expiry)
        {
            const auto rc = removeExpiryIndex(bucketId, *expiry);
            if (rc != 0)
                return rc;
        }
    }

    // Entries of large collections
    if (newest != entries.end() && (newest->type == KeyType::Hash || newest->type == KeyType::Set || newest->type == KeyType::ZSet) &&
        anyLargeCollections.load(std::memory_order_relaxed))
    {
        const auto rc = deleteCollectionEntries(bucketId, key);
        if (rc != 0)
            return rc;
    }

    for (const auto& entry : entries)
    {
        if (!entry.tombstone)
            existed = true;

        if (entry.expiry)
        {
            const auto rc = removeExpiryIndex(bucketId, *entry.expiry);
            if (rc != 0)
                return rc;
        }

        bool deleted;
        const auto rc = deleteVersion(bucketId, key, entry.version, false, deleted);
        if (rc != 0)
            return rc;
    }

    return 0;
}

// Small hash, set or sorted set, serialized inline in the object metadata (ObjMetadata::redisData) of the value: varint
// format version (1), varint number of elements, then the elements (hash: field, value; set: member; sorted set:
// member, score as little endian double). Strings are varint length + data. Larger collections are stored as one entry
// per element (see LargeCollOps)
struct RedisServer::Collection
{
    KeyType type = KeyType::None;
    std::map<std::string, std::string> hash;
    std::set<std::string> set;
    std::map<std::string, double> zscores;
    // Ordered by (score, member)
    std::set<std::pair<double, std::string>> zorder;
    std::optional<ExpiryInfo> expiry;

    size_t size() const
    {
        switch (type)
        {
            case KeyType::Hash:
                return hash.size();
            case KeyType::Set:
                return set.size();
            case KeyType::ZSet:
                return zscores.size();
            default:
                return 0;
        }
    }

    // Sets the score of a member. Returns true if it was added
    bool zset(const std::string& member, const double score)
    {
        auto it = zscores.find(member);
        if (it != zscores.end())
        {
            zorder.erase({ it->second, member });
            it->second = score;
            zorder.insert({ score, member });
            return false;
        }
        zscores[member] = score;
        zorder.insert({ score, member });
        return true;
    }

    bool zrem(const std::string& member)
    {
        auto it = zscores.find(member);
        if (it == zscores.end())
            return false;
        zorder.erase({ it->second, member });
        zscores.erase(it);
        return true;
    }

    std::string serialize() const
    {
        CWData data;
        data.addVarInt(1);
        data.addVarInt(static_cast<int64_t>(size()));
        switch (type)
        {
            case KeyType::Hash:
                for (const auto& [field, value] : hash)
                {
                    data.addString2(field);
                    data.addString2(value);
                }
                break;
            case KeyType::Set:
                for (const auto& member : set)
                    data.addString2(member);
                break;
            case KeyType::ZSet:
                for (const auto& [member, score] : zscores)
                {
                    data.addString2(member);
                    data.addDouble(score);
                }
                break;
            default:
                break;
        }
        return std::string(data.getDataPtr(), data.getDataSize());
    }

    bool deserialize(const std::string& str)
    {
        CRData data(str.data(), str.size());
        int64_t format, num;
        if (!data.getVarInt(&format) || format != 1 || !data.getVarInt(&num) || num < 0)
            return false;
        for (int64_t i = 0; i < num; ++i)
        {
            std::string first;
            if (!data.getStr2(&first))
                return false;
            switch (type)
            {
                case KeyType::Hash:
                {
                    std::string value;
                    if (!data.getStr2(&value))
                        return false;
                    hash.emplace(std::move(first), std::move(value));
                    break;
                }
                case KeyType::Set:
                    set.insert(std::move(first));
                    break;
                case KeyType::ZSet:
                {
                    double score;
                    if (!data.getDouble(&score))
                        return false;
                    zset(first, score);
                    break;
                }
                default:
                    return false;
            }
        }
        return true;
    }
};

// Operations on a hash, set or sorted set, either small (in memory, stored inline in the metadata of the value) or
// large (one entry per element in the counters bucket)
class RedisServer::CollOps
{
public:
    virtual ~CollOps() = default;

    // Hash value of field. Sorted set: score (in value) of member
    virtual std::optional<std::string> get(const std::string& field, int& err) = 0;
    virtual bool contains(const std::string& field, int& err) = 0;
    // Sets hash field (value) or adds set member (value ignored). added is set if the field is new
    virtual int set(const std::string& field, const std::string& value, bool& added) = 0;
    virtual int remove(const std::string& field, bool& removed) = 0;
    virtual std::optional<double> zscore(const std::string& member, int& err) = 0;
    virtual int zset(const std::string& member, const double score, bool& added) = 0;
    virtual int64_t count(int& err) = 0;
    // Hash/set elements (value is empty for sets). f returns false to stop. Returns false on error
    virtual bool forEach(const std::function<bool(const std::string&, const std::string&)>& f) = 0;
    // Sorted set elements ordered by score, starting at score from. f returns false to stop. Returns false on error
    virtual bool forEachByScore(const std::optional<double> from, const std::function<bool(double, const std::string&)>& f) = 0;
};

class RedisServer::SmallCollOps : public CollOps
{
public:
    explicit SmallCollOps(Collection& coll)
        : coll(coll)
    {
    }

    std::optional<std::string> get(const std::string& field, int& /*err*/) override
    {
        auto it = coll.hash.find(field);
        if (it == coll.hash.end())
            return std::nullopt;
        return it->second;
    }

    bool contains(const std::string& field, int& /*err*/) override
    {
        switch (coll.type)
        {
            case KeyType::Hash:
                return coll.hash.contains(field);
            case KeyType::Set:
                return coll.set.contains(field);
            default:
                return coll.zscores.contains(field);
        }
    }

    int set(const std::string& field, const std::string& value, bool& added) override
    {
        if (coll.type == KeyType::Hash)
            added = coll.hash.insert_or_assign(field, value).second;
        else
            added = coll.set.insert(field).second;
        dirty = true;
        return 0;
    }

    int remove(const std::string& field, bool& removed) override
    {
        switch (coll.type)
        {
            case KeyType::Hash:
                removed = coll.hash.erase(field) > 0;
                break;
            case KeyType::Set:
                removed = coll.set.erase(field) > 0;
                break;
            default:
                removed = coll.zrem(field);
                break;
        }
        dirty = dirty || removed;
        return 0;
    }

    std::optional<double> zscore(const std::string& member, int& /*err*/) override
    {
        auto it = coll.zscores.find(member);
        if (it == coll.zscores.end())
            return std::nullopt;
        return it->second;
    }

    int zset(const std::string& member, const double score, bool& added) override
    {
        added = coll.zset(member, score);
        dirty = true;
        return 0;
    }

    int64_t count(int& /*err*/) override
    {
        return static_cast<int64_t>(coll.size());
    }

    bool forEach(const std::function<bool(const std::string&, const std::string&)>& f) override
    {
        static const std::string empty;
        if (coll.type == KeyType::Hash)
        {
            for (const auto& [field, value] : coll.hash)
            {
                if (!f(field, value))
                    break;
            }
        }
        else
        {
            for (const auto& member : coll.set)
            {
                if (!f(member, empty))
                    break;
            }
        }
        return true;
    }

    bool forEachByScore(const std::optional<double> from, const std::function<bool(double, const std::string&)>& f) override
    {
        auto it = from ? coll.zorder.lower_bound({ *from, std::string() }) : coll.zorder.begin();
        for (; it != coll.zorder.end(); ++it)
        {
            if (!f(it->first, it->second))
                break;
        }
        return true;
    }

    Collection& coll;
    bool dirty = false;
};

// Entries of a large collection in the counters bucket (version 0):
// - Hash: prefix + field -> md5sum 'h' + long field (string) + value
// - Set: prefix + member -> md5sum 's' + long member (string)
// - Sorted set: prefix + 'm' + member -> md5sum 'z' + score (double) + long member (string), and
//   prefix + 's' + sortable score + member -> md5sum 'i' + member (string)
// with prefix = collectionPrefix() and fields/members encoded with encodeField(). The number of hash/set entries and
// sorted set member entries is counted by the storage engine (collectionCounterKey())
class RedisServer::LargeCollOps : public CollOps
{
public:
    LargeCollOps(RedisServer& server, const int64_t bucketId, const std::string& key, const KeyType type)
        : server(server), sfs(server.sfs), countersBucket(buckets::getCountersBucket(bucketId)), key(key), type(type), prefix(collectionPrefix(type, key))
    {
    }

    std::optional<std::string> get(const std::string& field, int& err) override
    {
        const auto md5sum = readEntry(fieldFn(field), err);
        if (!md5sum)
            return std::nullopt;
        CRData data(md5sum->data(), md5sum->size());
        char tag;
        std::string longField;
        if (!data.getChar(&tag) || tag != 'h' || !data.getStr2(&longField) || (isLongField(field) && longField != field))
            return std::nullopt;
        return std::string(data.getCurrDataPtr(), data.getLeft());
    }

    bool contains(const std::string& field, int& err) override
    {
        if (type == KeyType::Hash)
            return get(field, err).has_value();
        if (type == KeyType::ZSet)
            return zscore(field, err).has_value();
        const auto md5sum = readEntry(fieldFn(field), err);
        if (!md5sum)
            return false;
        CRData data(md5sum->data(), md5sum->size());
        char tag;
        std::string longField;
        return data.getChar(&tag) && data.getStr2(&longField) && (!isLongField(field) || longField == field);
    }

    int set(const std::string& field, const std::string& value, bool& added) override
    {
        int err = 0;
        added = !contains(field, err);
        if (err != 0)
            return err;
        CWData data;
        data.addChar(type == KeyType::Hash ? 'h' : 's');
        data.addString2(isLongField(field) ? field : std::string());
        if (type == KeyType::Hash)
            data.addBuffer(value.data(), value.size());
        return writeEntry(fieldFn(field), std::string_view(data.getDataPtr(), data.getDataSize()));
    }

    int remove(const std::string& field, bool& removed) override
    {
        int err = 0;
        removed = false;
        if (type == KeyType::ZSet)
        {
            const auto score = zscore(field, err);
            if (err != 0 || !score)
                return err;
            err = deleteEntry(scoreFn(*score, field));
            if (err != 0)
                return err;
        }
        else if (!contains(field, err))
        {
            return err;
        }

        err = deleteEntry(fieldFn(field));
        removed = err == 0;
        return err;
    }

    std::optional<double> zscore(const std::string& member, int& err) override
    {
        const auto md5sum = readEntry(fieldFn(member), err);
        if (!md5sum)
            return std::nullopt;
        CRData data(md5sum->data(), md5sum->size());
        char tag;
        double score;
        std::string longField;
        if (!data.getChar(&tag) || tag != 'z' || !data.getDouble(&score) || !data.getStr2(&longField) ||
            (isLongField(member) && longField != member))
            return std::nullopt;
        return score;
    }

    int zset(const std::string& member, const double score, bool& added) override
    {
        int err = 0;
        const auto prev = zscore(member, err);
        if (err != 0)
            return err;
        added = !prev;
        if (prev)
        {
            err = deleteEntry(scoreFn(*prev, member));
            if (err != 0)
                return err;
        }

        CWData memberData;
        memberData.addChar('z');
        memberData.addDouble(score);
        memberData.addString2(isLongField(member) ? member : std::string());
        err = writeEntry(fieldFn(member), std::string_view(memberData.getDataPtr(), memberData.getDataSize()));
        if (err != 0)
            return err;

        CWData indexData;
        indexData.addChar('i');
        indexData.addString2(member);
        return writeEntry(scoreFn(score, member), std::string_view(indexData.getDataPtr(), indexData.getDataSize()));
    }

    int64_t count(int& err) override
    {
        const auto res = sfs.read_prepare(make_key(collectionCounterKey(key), countersBucket, 0), SingleFileStorage::ReadSkipAddReading);
        if (res.err == ENOENT)
            return 0;
        if (res.err != 0)
        {
            err = res.err;
            return 0;
        }
        CRData data(res.md5sum.data(), res.md5sum.size());
        int64_t count = 0;
        data.getVarInt(&count);
        return count;
    }

    bool forEach(const std::function<bool(const std::string&, const std::string&)>& f) override
    {
        return iterate(prefix, [&](const std::string_view suffix, const std::string& md5sum)
                       {
                           CRData data(md5sum.data(), md5sum.size());
                           char tag;
                           std::string longField;
                           if (suffix.empty() || !data.getChar(&tag) || !data.getStr2(&longField))
                               return true;
                           const std::string field = suffix[0] == '\1' ? longField : std::string(suffix.substr(1));
                           const std::string value = type == KeyType::Hash ? std::string(data.getCurrDataPtr(), data.getLeft()) : std::string();
                           return f(field, value); });
    }

    bool forEachByScore(const std::optional<double> from, const std::function<bool(double, const std::string&)>& f) override
    {
        const auto scorePrefix = prefix + "s";
        return iterate(scorePrefix, [&](const std::string_view suffix, const std::string& md5sum)
                       {
                           CRData data(md5sum.data(), md5sum.size());
                           char tag;
                           std::string member;
                           if (suffix.size() < 8 || !data.getChar(&tag) || !data.getStr2(&member))
                               return true;
                           return f(scoreFromSortable(suffix.substr(0, 8)), member); }, from ? std::optional<std::string>(scorePrefix + sortableScore(*from)) : std::nullopt);
    }

    // Writes all elements of a small collection
    int addAll(const Collection& coll)
    {
        bool added;
        switch (type)
        {
            case KeyType::Hash:
                for (const auto& [field, value] : coll.hash)
                {
                    if (const auto rc = set(field, value, added); rc != 0)
                        return rc;
                }
                break;
            case KeyType::Set:
                for (const auto& member : coll.set)
                {
                    if (const auto rc = set(member, std::string(), added); rc != 0)
                        return rc;
                }
                break;
            default:
                for (const auto& [member, score] : coll.zscores)
                {
                    if (const auto rc = zset(member, score, added); rc != 0)
                        return rc;
                }
                break;
        }
        return 0;
    }

private:
    std::string fieldFn(const std::string_view field) const
    {
        return make_key(prefix + (type == KeyType::ZSet ? "m" : "") + encodeField(field), countersBucket, 0);
    }

    std::string scoreFn(const double score, const std::string_view member) const
    {
        return make_key(prefix + "s" + sortableScore(score) + encodeField(member), countersBucket, 0);
    }

    std::optional<std::string> readEntry(const std::string& fn, int& err)
    {
        auto res = sfs.read_prepare(fn, SingleFileStorage::ReadSkipAddReading);
        if (res.err == ENOENT)
            return std::nullopt;
        if (res.err != 0)
        {
            err = res.err;
            return std::nullopt;
        }
        return std::move(res.md5sum);
    }

    int writeEntry(const std::string& fn, const std::string_view md5sum)
    {
        if (currentTx != nullptr)
            currentTx->largeWrites = true;
        const auto lastModified = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        return sfs.write_finalize(fn, {}, lastModified, md5sum, false, true);
    }

    int deleteEntry(const std::string& fn)
    {
        if (currentTx != nullptr)
            currentTx->largeWrites = true;
        const auto rc = sfs.del(fn, SingleFileStorage::DelAction::Del, false);
        return rc == ENOENT ? 0 : rc;
    }

    // Iterates over entries starting with iterPrefix (starting at start). f(key suffix after iterPrefix, md5sum)
    template <typename F>
    bool iterate(const std::string& iterPrefix, F f, const std::optional<std::string>& start = std::nullopt)
    {
        server.syncListing();

        SingleFileStorage::ListIterData iterData;
        SCOPE_EXIT { sfs.list_iter_stop(iterData); };

        const auto startKey = make_key(start ? *start : iterPrefix, countersBucket, std::numeric_limits<int64_t>::max());
        const auto bucket = countersBucket;
        if (!sfs.list_iter_start(startKey, startKey, [bucket, &iterPrefix](const std::string& iterKey)
                                 {
                                     const auto keyInfo = extractKeyInfoView(iterKey);
                                     return keyInfo.bucketId == bucket && keyInfo.key.starts_with(iterPrefix); }, iterData))
        {
            return false;
        }

        while (true)
        {
            std::string keyBin, md5sum;
            int64_t offset, size, lastModified;
            std::vector<SingleFileStorage::SPunchItem> extraExts;
            if (!sfs.iter_curr_val(keyBin, offset, size, extraExts, lastModified, md5sum, iterData.curr))
                return true;

            const auto keyInfo = extractKeyInfoView(keyBin);
            if (keyInfo.bucketId != countersBucket || !keyInfo.key.starts_with(iterPrefix))
                return true;

            if (!f(keyInfo.key.substr(iterPrefix.size()), md5sum))
                return true;

            if (!sfs.list_iter_next(iterData))
                return false;
        }
    }

    RedisServer& server;
    SingleFileStorage& sfs;
    const int64_t countersBucket;
    const std::string key;
    const KeyType type;
    const std::string prefix;
};

// Shared lock of a key (field-level modifications of large collections). Does nothing in a transaction/script
class RedisServer::SharedKeyLock
{
public:
    SharedKeyLock(RedisServer& server, const std::string& key) NO_THREAD_SAFETY_ANALYSIS
    {
        if (currentTx != nullptr)
            return;
        lock = &server.keyLock(key);
        lock->lock_shared();
    }

    ~SharedKeyLock() NO_THREAD_SAFETY_ANALYSIS
    {
        if (lock != nullptr)
            lock->unlock_shared();
    }

    SharedKeyLock(const SharedKeyLock&) = delete;
    SharedKeyLock& operator=(const SharedKeyLock&) = delete;

private:
    std::shared_mutex* lock = nullptr;
};

// Locks of fields of a key (with the key locked shared). Does nothing in a transaction/script
class RedisServer::FieldLocks
{
public:
    FieldLocks(RedisServer& server, const std::string& key, const std::vector<std::string>& fields) NO_THREAD_SAFETY_ANALYSIS
        : server(server)
    {
        if (currentTx != nullptr)
            return;
        for (const auto& field : fields)
            idxs.push_back(std::hash<std::string>{}(key + '\0' + field) % server.fieldLocks.size());
        std::sort(idxs.begin(), idxs.end());
        idxs.erase(std::unique(idxs.begin(), idxs.end()), idxs.end());
        for (const auto idx : idxs)
            server.fieldLocks[idx].lock();
    }

    ~FieldLocks() NO_THREAD_SAFETY_ANALYSIS
    {
        for (auto it = idxs.rbegin(); it != idxs.rend(); ++it)
            server.fieldLocks[*it].unlock();
    }

    FieldLocks(const FieldLocks&) = delete;
    FieldLocks& operator=(const FieldLocks&) = delete;

private:
    RedisServer& server;
    std::vector<size_t> idxs;
};

struct RedisServer::CollState
{
    bool exists = false;
    // Stored with one entry per element (otherwise inline, in small)
    bool large = false;
    Collection small;
    std::optional<ExpiryInfo> expiry;
};
// Group lock of a key (see collGroupLocks). Does nothing in a transaction/script
class RedisServer::CollGroupLock
{
public:
    static constexpr int writers = 0;
    static constexpr int readers = 1;

    CollGroupLock(RedisServer& server, const std::string& key, const int group)
        : group(group)
    {
        if (currentTx != nullptr)
            return;
        mutex = &server.collGroupLocks[server.keyLockIdx(key)];
        mutex->lock(group);
    }

    ~CollGroupLock()
    {
        if (mutex != nullptr)
            mutex->unlock(group);
    }

    CollGroupLock(const CollGroupLock&) = delete;
    CollGroupLock& operator=(const CollGroupLock&) = delete;

private:
    GroupMutex* mutex = nullptr;
    const int group;
};

// Released in reverse order
struct RedisServer::CollLocks
{
    std::optional<KeyLock> exclusive;
    std::optional<SharedKeyLock> shared;
    std::optional<CollGroupLock> group;
    std::optional<FieldLocks> fields;
};

std::optional<std::string> RedisServer::openCollection(const std::string& key, const KeyType type, CollState& cs)
{
    cs = CollState();
    cs.small.type = type;
    try
    {
        ObjectReader reader(sfs, bucketName, key);
        const auto objMetadata = reader.GetObjMetadata();
        const auto expiry = expiryFromObjMetadata(objMetadata);
        // Expired (but not deleted yet)
        if (isExpired(expiry))
            return std::nullopt;
        if (readerKeyType(reader) != type)
            return resp::wrongType;

        cs.exists = true;
        cs.expiry = expiry;
        cs.small.expiry = expiry;
        if (objMetadata == nullptr || !objMetadata->redisData)
        {
            cs.large = true;
            return std::nullopt;
        }

        if (!cs.small.deserialize(*objMetadata->redisData))
            return resp::error(fmt::format("ERR corrupted value of key '{}'", key));
        return std::nullopt;
    }
    catch (const ObjectReadError& e)
    {
        if (e.notFound())
            return std::nullopt;

        XLOGF(WARN, "Error reading Redis key {}: {}", key, e.what());
        return resp::error(fmt::format("ERR {}", e.what()));
    }
}

int RedisServer::deleteCollectionEntries(const int64_t bucketId, const std::string& key)
{
    const auto countersBucket = buckets::getCountersBucket(bucketId);
    for (const auto type : { KeyType::Hash, KeyType::Set, KeyType::ZSet })
    {
        if (auto err = deleteEntries(countersBucket, collectionPrefix(type, key), nullptr))
            return EIO;
    }
    return 0;
}

int RedisServer::writeSmallCollection(const int64_t bucketId, const std::string& key, const Collection& coll, bool& converted)
{
    converted = false;
    if (coll.size() == 0)
    {
        bool existed;
        return deleteKey(bucketId, key, existed);
    }

    const auto data = coll.serialize();
    if (static_cast<int64_t>(coll.size()) <= FLAGS_redis_collection_inline_max_entries &&
        static_cast<int64_t>(data.size()) <= FLAGS_redis_collection_inline_max_bytes)
    {
        return setValue(bucketId, key, ValueArg{}, collectionContentType(coll.type), coll.expiry, &data);
    }

    // Convert to large collection. Remove entries left over (e.g. if the key was deleted via S3)
    anyLargeCollections = true;
    converted = true;
    auto rc = deleteCollectionEntries(bucketId, key);
    if (rc != 0)
        return rc;
    LargeCollOps large(*this, bucketId, key, coll.type);
    rc = large.addAll(coll);
    if (rc != 0)
        return rc;
    return setValue(bucketId, key, ValueArg{}, collectionContentType(coll.type), coll.expiry, nullptr);
}

bool RedisServer::locksCollection(const std::string& key, const KeyType type, const std::vector<std::string>& fields, CollLocks& locks,
                                  CollState& cs, std::optional<std::string>& err)
{
    if (!fields.empty())
    {
        // Large collections: Lock the key shared and the modified fields
        locks.shared.emplace(*this, key);
        err = openCollection(key, type, cs);
        if (err)
            return false;
        if (cs.large)
        {
            locks.group.emplace(*this, key, CollGroupLock::writers);
            locks.fields.emplace(*this, key, fields);
            // For WATCH
            touchKeyLock(keyLockIdx(key));
            return true;
        }
        locks.shared.reset();
    }

    locks.exclusive.emplace(*this, key);
    err = openCollection(key, type, cs);
    return !err;
}

std::optional<std::string> RedisServer::openCollectionForRead(const std::string& key, const KeyType type, const bool multiElement, CollLocks& locks,
                                                              CollState& cs)
{
    if (!multiElement)
        return openCollection(key, type, cs);

    // Reads of multiple elements of large collections exclude field modifications (atomic). Small collections are
    // read from one object
    locks.shared.emplace(*this, key);
    auto err = openCollection(key, type, cs);
    if (!err && cs.large)
        locks.group.emplace(*this, key, CollGroupLock::readers);
    return err;
}

void RedisServer::deleteIfEmpty(const int64_t bucketId, const std::string& key, const KeyType type)
{
    KeyLock lock(*this, key);
    CollState cs;
    if (openCollection(key, type, cs) || !cs.exists || !cs.large)
        return;
    LargeCollOps large(*this, bucketId, key, type);
    int err = 0;
    if (large.count(err) == 0 && err == 0)
    {
        bool existed;
        if (deleteKey(bucketId, key, existed) == 0 && !commit())
            XLOGF(WARN, "Error committing deletion of empty Redis key {}", key);
    }
}

std::optional<RedisServer::ExpiryInfo> RedisServer::listExpiry(const int64_t bucketId, const std::string& key, int& err)
{
    const auto res = sfs.read_prepare(make_key(key, buckets::getCountersBucket(bucketId), 0), SingleFileStorage::ReadSkipAddReading);
    if (res.err == ENOENT)
        return std::nullopt;
    if (res.err != 0)
    {
        err = res.err;
        return std::nullopt;
    }

    CRData rdata(res.md5sum.data(), res.md5sum.size());
    int64_t count;
    ExpiryInfo expiry;
    if (!rdata.getVarInt(&count) || !rdata.getVarInt(&expiry.ms) || !rdata.getVarInt(&expiry.indexVersion))
        return std::nullopt;
    return expiry;
}

std::optional<RedisServer::ExpiryInfo> RedisServer::entryExpiry(const int64_t bucketId, const std::string& key, const Entry& entry, int& err)
{
    if (entry.isList)
        return listExpiry(bucketId, key, err);
    return entry.expiry;
}

std::optional<RedisServer::ExpiryInfo> RedisServer::getExpiry(const int64_t bucketId, const std::string& key, int& err)
{
    const auto entry = findElement(bucketId, key, true, err);
    if (!entry)
        return std::nullopt;
    return entryExpiry(bucketId, key, *entry, err);
}

int RedisServer::addExpiryIndex(const int64_t bucketId, const std::string& key, const int64_t expiryMs, ExpiryInfo& expiry)
{
    // Unique version (> 0), so entries with the same expiry time differ
    const auto indexVersion = sfs.get_next_version();

    const auto rc = sfs.write_finalize(make_key(expiryIndexKey(expiryMs), buckets::getCountersBucket(bucketId), indexVersion), {}, expiryMs, key, false, true);
    if (rc != 0)
        return rc;

    expiry = ExpiryInfo{ .ms = expiryMs, .indexVersion = indexVersion };

    std::scoped_lock lock(expiryFlagMutex);
    ++expiryGeneration;
    anyExpiries = true;
    return 0;
}

int RedisServer::removeExpiryIndex(const int64_t bucketId, const ExpiryInfo& expiry)
{
    const auto rc = sfs.del(make_key(expiryIndexKey(expiry.ms), buckets::getCountersBucket(bucketId), expiry.indexVersion), SingleFileStorage::DelAction::Del, false);
    return rc == ENOENT ? 0 : rc;
}

int RedisServer::modifyExpiry(const int64_t bucketId, const std::string& key, const Entry& entry, const std::optional<ExpiryInfo>& expiry)
{
    CWData param;
    if (entry.isList)
    {
        // Stored in the counter of the list
        param.addChar(metadata_modify_counter_tail);
        if (expiry)
        {
            param.addVarInt(expiry->ms);
            param.addVarInt(expiry->indexVersion);
        }
        return sfs.modify_metadata(make_key(key, buckets::getCountersBucket(bucketId), 0), std::string_view(param.getDataPtr(), param.getDataSize()));
    }

    param.addChar(metadata_modify_redis_expiry);
    param.addChar(expiry ? 1 : 0);
    if (expiry)
    {
        param.addVarInt(expiry->ms);
        param.addVarInt(expiry->indexVersion);
    }
    return sfs.modify_metadata(make_key(key, bucketId, entry.version), std::string_view(param.getDataPtr(), param.getDataSize()));
}

int RedisServer::setExpiry(const int64_t bucketId, const std::string& key, const int64_t expiryMs)
{
    int err = 0;
    const auto entry = findElement(bucketId, key, true, err);
    if (err != 0)
        return err;
    if (!entry)
        return ENOENT;

    const auto prev = entryExpiry(bucketId, key, *entry, err);
    if (err != 0)
        return err;
    if (prev)
    {
        const auto rc = removeExpiryIndex(bucketId, *prev);
        if (rc != 0)
            return rc;
    }

    ExpiryInfo expiry;
    auto rc = addExpiryIndex(bucketId, key, expiryMs, expiry);
    if (rc != 0)
        return rc;

    return modifyExpiry(bucketId, key, *entry, expiry);
}

int RedisServer::clearExpiry(const int64_t bucketId, const std::string& key, bool* removed)
{
    if (removed)
        *removed = false;

    int err = 0;
    const auto entry = findElement(bucketId, key, true, err);
    if (err != 0)
        return err;
    if (!entry)
        return 0;
    const auto prev = entryExpiry(bucketId, key, *entry, err);
    if (err != 0)
        return err;
    if (!prev)
        return 0;

    auto rc = removeExpiryIndex(bucketId, *prev);
    if (rc != 0)
        return rc;

    rc = modifyExpiry(bucketId, key, *entry, std::nullopt);
    if (rc != 0)
        return rc;

    if (removed)
        *removed = true;
    return 0;
}

std::optional<RedisServer::ExpiryInfo> RedisServer::removeCurrentExpiry(const int64_t bucketId, const std::string& key, int& err)
{
    if (!anyExpiries.load(std::memory_order_relaxed))
        return std::nullopt;

    const auto expiry = getExpiry(bucketId, key, err);
    if (err != 0 || !expiry)
        return std::nullopt;

    err = removeExpiryIndex(bucketId, *expiry);
    return expiry;
}

bool RedisServer::expireIfNeeded(const int64_t bucketId, const std::string& key)
{
    int err = 0;
    if (!isExpired(getExpiry(bucketId, key, err)))
        return false;

    {
        KeyLock lock(*this, key);
        if (!isExpired(getExpiry(bucketId, key, err)))
            return false;

        bool existed;
        const auto rc = deleteKey(bucketId, key, existed);
        if (rc != 0)
        {
            XLOGF(WARN, "Error deleting expired Redis key {}: {}", key, rc);
            return false;
        }
    }

    if (!commit())
        XLOGF(WARN, "Error committing deletion of expired Redis key {}", key);

    notifyKey(key);
    return true;
}

void RedisServer::expireKeys()
{
    const auto bucketInfo = buckets::getBucketInfo(bucketName);
    if (!bucketInfo)
        return;

    uint64_t generation;
    {
        std::scoped_lock lock(expiryFlagMutex);
        generation = expiryGeneration;
    }

    constexpr size_t maxBatch = 1000;
    const auto countersBucket = buckets::getCountersBucket(bucketInfo->id);

    while (true)
    {
        struct IndexEntry
        {
            std::string key;
            ExpiryInfo expiry;
        };
        std::vector<IndexEntry> expired;
        bool foundAny = false;
        const auto now = nowMs();
        {
            SingleFileStorage::ListIterData iterData;
            SCOPE_EXIT { sfs.list_iter_stop(iterData); };

            const auto start = make_key(expiryIndexPrefix, countersBucket, std::numeric_limits<int64_t>::max());
            if (!sfs.list_iter_start(start, start, [countersBucket](const std::string& iterKey)
                                     {
                                         const auto keyInfo = extractKeyInfoView(iterKey);
                                         return keyInfo.bucketId == countersBucket && keyInfo.key.starts_with(expiryIndexPrefix); }, iterData))
            {
                XLOGF(WARN, "Error listing Redis key expiries");
                return;
            }

            while (expired.size() < maxBatch)
            {
                std::string keyBin, md5sum;
                int64_t offset, size, lastModified;
                std::vector<SingleFileStorage::SPunchItem> extraExts;
                if (!sfs.iter_curr_val(keyBin, offset, size, extraExts, lastModified, md5sum, iterData.curr))
                    break;

                const auto keyInfo = extractKeyInfoView(keyBin);
                if (keyInfo.bucketId != countersBucket || !keyInfo.key.starts_with(expiryIndexPrefix))
                    break;

                const auto expiryMs = parseExpiryIndexKey(keyInfo.key);
                // Skip counters of keys which happen to start with the prefix (only possible via S3)
                if (expiryMs && keyInfo.version != 0)
                {
                    foundAny = true;
                    if (*expiryMs > now)
                        break;
                    expired.push_back(IndexEntry{ .key = md5sum, .expiry = ExpiryInfo{ .ms = *expiryMs, .indexVersion = keyInfo.version } });
                }

                if (!sfs.list_iter_next(iterData))
                    break;
            }
        }

        if (!foundAny)
        {
            std::scoped_lock lock(expiryFlagMutex);
            if (generation == expiryGeneration)
                anyExpiries = false;
            return;
        }

        if (expired.empty())
            return;

        std::shared_lock globalLockShared(globalLock);
        for (const auto& entry : expired)
        {
            KeyLock lock(*this, entry.key);
            int err = 0;
            const auto curr = getExpiry(bucketInfo->id, entry.key, err);
            if (err != 0)
                continue;

            if (curr == entry.expiry)
            {
                bool existed;
                const auto rc = deleteKey(bucketInfo->id, entry.key, existed);
                if (rc != 0)
                    XLOGF(WARN, "Error deleting expired Redis key {}: {}", entry.key, rc);
            }
            else
            {
                // Stale index entry (e.g. key was deleted via S3)
                removeExpiryIndex(bucketInfo->id, entry.expiry);
            }
        }

        if (!commit())
            XLOGF(WARN, "Error committing deletion of expired Redis keys");

        for (const auto& entry : expired)
            notifyKey(entry.key);

        if (expired.size() < maxBatch)
            return;
    }
}

void RedisServer::expiryThreadFunc()
{
    while (true)
    {
        {
            CondVarLock lock(expiryThreadMutex);
            if (expiryThreadStop)
                return;
            lock.waitFor(expiryThreadCond, std::chrono::milliseconds(100));
            if (expiryThreadStop)
                return;
        }

        try
        {
            expireKeys();
        }
        catch (const std::exception& e)
        {
            XLOGF(ERR, "Exception expiring Redis keys: {}", e.what());
        }
    }
}

namespace
{
    std::string pubsubMessage(const bool resp3, const std::string_view type, const std::optional<std::string_view> pattern,
                              const std::string_view channel, const std::string_view message)
    {
        std::string ret = fmt::format("{}{}\r\n", resp3 ? '>' : '*', pattern ? 4 : 3);
        resp::appendBulk(ret, type);
        if (pattern)
            resp::appendBulk(ret, *pattern);
        resp::appendBulk(ret, channel);
        resp::appendBulk(ret, message);
        return ret;
    }
} // namespace

int64_t RedisServer::publish(const std::string& channel, const std::string& message)
{
    std::vector<std::pair<std::shared_ptr<RedisConnection>, std::string>> deliveries;
    {
        std::scoped_lock lock(pubsubMutex);
        auto it = channelSubs.find(channel);
        if (it != channelSubs.end())
        {
            for (const auto& [ptr, sub] : it->second)
            {
                if (auto conn = sub.conn.lock())
                    deliveries.emplace_back(std::move(conn), pubsubMessage(sub.resp3, "message", std::nullopt, channel, message));
            }
        }

        for (const auto& [pattern, subs] : patternSubs)
        {
            if (!globMatch(pattern, channel))
                continue;
            for (const auto& [ptr, sub] : subs)
            {
                if (auto conn = sub.conn.lock())
                    deliveries.emplace_back(std::move(conn), pubsubMessage(sub.resp3, "pmessage", pattern, channel, message));
            }
        }
    }

    const auto num = static_cast<int64_t>(deliveries.size());
    for (auto& [conn, msg] : deliveries)
        RedisConnection::push(std::move(conn), std::move(msg));
    return num;
}

void RedisServer::subscribe(const std::string& name, const bool pattern, const bool resp3, const std::shared_ptr<RedisConnection>& conn)
{
    std::scoped_lock lock(pubsubMutex);
    // Connection was closed (and unsubscribed from everything) already
    if (conn->isClosed())
        return;
    conn->markSubscribed();
    auto& subs = pattern ? patternSubs : channelSubs;
    subs[name][conn.get()] = Subscriber{ .conn = conn, .resp3 = resp3 };
}

void RedisServer::unsubscribe(const std::string& name, const bool pattern, const RedisConnection* conn)
{
    std::scoped_lock lock(pubsubMutex);
    auto& subs = pattern ? patternSubs : channelSubs;
    auto it = subs.find(name);
    if (it == subs.end())
        return;
    it->second.erase(conn);
    if (it->second.empty())
        subs.erase(it);
}

void RedisServer::unsubscribeAll(const RedisConnection* conn)
{
    std::scoped_lock lock(pubsubMutex);
    for (auto* subs : { &channelSubs, &patternSubs })
    {
        for (auto it = subs->begin(); it != subs->end();)
        {
            it->second.erase(conn);
            if (it->second.empty())
                it = subs->erase(it);
            else
                ++it;
        }
    }
}

RedisServer::ExecResult RedisServer::execMulti(ConnState& state, const std::shared_ptr<RedisConnection>& conn)
{
    auto queue = std::move(state.multiQueue);
    auto watched = std::move(state.watched);
    const bool error = state.multiError;
    state.multiQueue.clear();
    state.watched.clear();
    state.inMulti = false;
    state.multiError = false;

    if (error)
        return { resp::error("EXECABORT Transaction discarded because of previous errors.") };

    std::vector<std::string> keys;
    for (const auto& [key, epoch] : watched)
        keys.push_back(key);
    std::vector<std::string> names;
    names.reserve(queue.size());
    for (const auto& cmd : queue)
    {
        names.push_back(toUpper(cmd.args[0]));
        auto cmdKeys = commandKeys(names.back(), cmd.args);
        keys.insert(keys.end(), std::make_move_iterator(cmdKeys.begin()), std::make_move_iterator(cmdKeys.end()));
    }

    TxContext tx;
    ReplyBuilder reply;
    {
        MultiKeyLock lock(*this, keys);
        for (const auto& [key, epoch] : watched)
        {
            if (keyEpochs[keyLockIdx(key)].load() != epoch)
                return { resp::nullArray(state) };
        }
        lock.touch();

        currentTx = &tx;
        SCOPE_EXIT { currentTx = nullptr; };

        reply.append(resp::arrayHeader(queue.size()));
        for (size_t i = 0; i < queue.size(); ++i)
        {
            ExecResult res;
            try
            {
                res = execCommand(state, queue[i], names[i], conn);
            }
            catch (const std::exception& e)
            {
                XLOGF(ERR, "Exception executing Redis command in transaction: {}", e.what());
                res = ExecResult{ .reply = resp::error("ERR internal error") };
            }
            reply.appendResult(std::move(res));
        }

        // Commit modified entries of large collections before releasing the locks (see finishCollectionWrite)
        if (tx.needCommit && tx.largeWrites && !sfs.listings_include_uncommitted())
        {
            if (!S3Handler::commitStorage(sfs))
                return { resp::error("ERR storage commit failed") };
            tx.needCommit = false;
        }
    }

    if (tx.needCommit && !commit())
        return { resp::error("ERR storage commit failed") };

    return reply.finish();
}

RedisServer::ExecResult RedisServer::exec(ConnState& state, const RedisCommand& command, const std::shared_ptr<RedisConnection>& conn)
{
    const auto& args = command.args;
    assert(!args.empty());
    const auto cmd = toUpper(args[0]);

    XLOGF(DBG0, "Redis connection {} command {} argc {}", state.id, cmd, args.size());

    if (state.inMulti && cmd != "EXEC" && cmd != "DISCARD" && cmd != "MULTI" && cmd != "WATCH" && cmd != "QUIT" && cmd != "RESET")
    {
        if (!isKnownCommand(cmd))
        {
            state.multiError = true;
            return { resp::error(fmt::format("ERR unknown command '{}'", args[0].substr(0, 128))) };
        }
        if (cmd == "SUBSCRIBE" || cmd == "PSUBSCRIBE" || cmd == "UNSUBSCRIBE" || cmd == "PUNSUBSCRIBE")
        {
            state.multiError = true;
            return { resp::error(fmt::format("ERR Command {} is not allowed in transactions", toLower(cmd))) };
        }
        state.multiQueue.push_back(command);
        return { resp::simple("QUEUED") };
    }

    if (state.subscribed() && !state.resp3)
    {
        // RESP2 connections in subscribed state only accept pub/sub commands
        if (cmd == "PING")
        {
            std::string reply = resp::arrayHeader(2);
            resp::appendBulk(reply, "pong");
            resp::appendBulk(reply, args.size() >= 2 ? args[1] : std::string());
            return { reply };
        }
        if (cmd != "SUBSCRIBE" && cmd != "PSUBSCRIBE" && cmd != "UNSUBSCRIBE" && cmd != "PUNSUBSCRIBE" && cmd != "QUIT" && cmd != "RESET")
        {
            return { resp::error(fmt::format("ERR Can't execute '{}': only (P|S)SUBSCRIBE / (P|S)UNSUBSCRIBE / PING / QUIT / RESET are allowed in this context",
                                             toLower(cmd))) };
        }
    }

    if (!needsGlobalLock(state, cmd, args))
    {
        std::shared_lock lock(globalLock);
        auto res = execCommand(state, command, cmd, conn);
        if (!res.needsGlobalLock)
            return res;
    }

    // Script accessing undeclared keys. Run it exclusively (no other command runs concurrently)
    std::unique_lock lock(globalLock);
    inGlobalLock = true;
    SCOPE_EXIT { inGlobalLock = false; };
    return execCommand(state, command, cmd, conn);
}

bool RedisServer::isGlobalScript(const std::string& sha, const std::string* body)
{
    std::scoped_lock lock(scriptsMutex);
    if (globalScripts.empty())
        return false;
    return globalScripts.contains(body != nullptr ? sha1Hex(*body) : toLower(sha));
}

bool RedisServer::needsGlobalLock(const ConnState& state, const std::string& cmd, const std::vector<std::string>& args)
{
    if (FLAGS_redis_script_always_global && cmd.starts_with("EVAL"))
        return true;
    if ((cmd == "EVAL" || cmd == "EVAL_RO") && args.size() >= 2)
        return isGlobalScript(std::string(), &args[1]);
    if ((cmd == "EVALSHA" || cmd == "EVALSHA_RO") && args.size() >= 2)
        return isGlobalScript(args[1], nullptr);
    if (cmd == "EXEC" && state.inMulti)
    {
        for (const auto& queued : state.multiQueue)
        {
            const auto queuedCmd = toUpper(queued.args[0]);
            if ((queuedCmd.starts_with("EVAL")) && needsGlobalLock(state, queuedCmd, queued.args))
                return true;
        }
    }
    return false;
}

void RedisServer::markGlobalScript(const std::string& sha)
{
    std::scoped_lock lock(scriptsMutex);
    if (globalScripts.insert(sha).second)
        XLOGF(INFO, "Lua script {} accesses undeclared keys. Running it with the global lock", sha);
}

RedisServer::ExecResult RedisServer::execCommand(ConnState& state, const RedisCommand& command, const std::string& cmd, const std::shared_ptr<RedisConnection>& conn)
{
    const auto& args = command.args;
    const auto argc = args.size();

    // Delete expired keys before accessing them
    if (anyExpiries.load(std::memory_order_relaxed) && cmd != "EXEC")
    {
        const auto keys = commandKeys(cmd, args);
        if (!keys.empty())
        {
            BucketCtx ctx;
            if (!getBucketCtx(ctx))
            {
                for (const auto& key : keys)
                {
                    if (!checkKey(key))
                        expireIfNeeded(ctx.id, key);
                }
            }
        }
    }

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
        state.inMulti = false;
        state.multiError = false;
        state.multiQueue.clear();
        state.watched.clear();
        if (state.subscribed())
        {
            state.channels.clear();
            state.patterns.clear();
            unsubscribeAll(conn.get());
        }
        return { resp::simple("RESET") };
    }
    if (cmd == "MULTI")
    {
        if (argc != 1)
            return { resp::wrongArgs("multi") };
        if (state.inMulti)
            return { resp::error("ERR MULTI calls can not be nested") };
        if (currentTx != nullptr)
            return { resp::error("ERR MULTI is not allowed in transactions or scripts") };
        state.inMulti = true;
        state.multiError = false;
        state.multiQueue.clear();
        return { resp::ok };
    }
    if (cmd == "EXEC")
    {
        if (argc != 1)
            return { resp::wrongArgs("exec") };
        if (!state.inMulti)
            return { resp::error("ERR EXEC without MULTI") };
        return execMulti(state, conn);
    }
    if (cmd == "DISCARD")
    {
        if (argc != 1)
            return { resp::wrongArgs("discard") };
        if (!state.inMulti)
            return { resp::error("ERR DISCARD without MULTI") };
        state.inMulti = false;
        state.multiError = false;
        state.multiQueue.clear();
        state.watched.clear();
        return { resp::ok };
    }
    if (cmd == "WATCH")
    {
        if (argc < 2)
            return { resp::wrongArgs("watch") };
        if (state.inMulti)
            return { resp::error("ERR WATCH inside MULTI is not allowed") };
        for (size_t i = 1; i < argc; ++i)
        {
            if (auto err = checkKey(args[i]))
                return { *err };
        }
        // Wait for running modifications of the keys
        MultiKeyLock lock(*this, args, 1, 1, false);
        for (size_t i = 1; i < argc; ++i)
            state.watched.emplace_back(args[i], keyEpochs[keyLockIdx(args[i])].load());
        return { resp::ok };
    }
    if (cmd == "UNWATCH")
    {
        state.watched.clear();
        return { resp::ok };
    }
    if (cmd == "TIME")
    {
        const auto us = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        std::string reply = resp::arrayHeader(2);
        resp::appendBulk(reply, std::to_string(us / 1000000));
        resp::appendBulk(reply, std::to_string(us % 1000000));
        return { reply };
    }
    if (cmd == "SETEX" || cmd == "PSETEX")
        return cmdSetex(state, command, cmd == "PSETEX");
    if (cmd == "GETDEL")
        return cmdGetdel(state, args);
    if (cmd == "EXPIRE" || cmd == "PEXPIRE" || cmd == "EXPIREAT" || cmd == "PEXPIREAT")
        return cmdExpire(state, args, cmd);
    if (cmd == "TTL" || cmd == "PTTL" || cmd == "EXPIRETIME" || cmd == "PEXPIRETIME")
        return cmdTtl(state, args, cmd);
    if (cmd == "PERSIST")
        return cmdPersist(state, args);
    if (cmd.size() > 1 && cmd[0] == 'H' && keySpecs().contains(cmd))
        return cmdHash(state, args, cmd);
    if (cmd == "SADD" || cmd == "SREM" || cmd == "SMEMBERS" || cmd == "SISMEMBER" || cmd == "SMISMEMBER" || cmd == "SCARD")
        return cmdSetType(state, args, cmd);
    if (cmd.size() > 1 && cmd[0] == 'Z' && keySpecs().contains(cmd))
        return cmdZset(state, args, cmd);
    if (cmd == "PUBLISH")
        return cmdPublish(state, args);
    if (cmd == "SUBSCRIBE" || cmd == "PSUBSCRIBE" || cmd == "UNSUBSCRIBE" || cmd == "PUNSUBSCRIBE")
        return cmdPubsub(state, args, cmd, conn);
    if (cmd == "EVAL" || cmd == "EVALSHA" || cmd == "EVAL_RO" || cmd == "EVALSHA_RO")
        return cmdEval(state, args, cmd, conn);
    if (cmd == "SCRIPT")
        return cmdScript(state, args);
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

    // Don't block the EventBase thread while a script holds the global lock
    std::shared_lock globalLockShared(globalLock, std::try_to_lock);
    if (!globalLockShared.owns_lock())
        return std::nullopt;

    try
    {
        // Newest version (one lookup). Not found if it is a delete marker
        // Reads of keys with uncommitted modifications would wait for the commit thread. Execute them via exec instead
        ObjectReader reader(sfs, bucketName, key, 0, ObjectReader::VersionSelect::Default, SingleFileStorage::ReadWithMmap,
                            SingleFileStorage::ReadFailUnsynced);
        // Expired keys are deleted via exec
        if (isExpired(expiryFromObjMetadata(reader.GetObjMetadata())))
            return std::nullopt;

        if (readerKeyType(reader) != KeyType::String)
            return ExecResult{ resp::wrongType };

        const auto size = reader.FileSize();
        if (size >= FLAGS_get_in_worker_threshold)
            return std::nullopt;

        if (FLAGS_test_inline_read_delay_ms > 0)
            std::this_thread::sleep_for(std::chrono::milliseconds(FLAGS_test_inline_read_delay_ms));

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

    bool nx = false, xx = false, get = false, keepTtl = false;
    std::optional<int64_t> expiryMs;
    for (size_t i = 3; i < args.size(); ++i)
    {
        const auto opt = toUpper(args[i]);
        if (opt == "NX")
            nx = true;
        else if (opt == "XX")
            xx = true;
        else if (opt == "GET")
            get = true;
        else if (opt == "KEEPTTL" && !expiryMs)
            keepTtl = true;
        else if ((opt == "EX" || opt == "PX" || opt == "EXAT" || opt == "PXAT") && !expiryMs && !keepTtl && i + 1 < args.size())
        {
            int64_t val;
            if (!parseInt64(args[i + 1], val))
                return { resp::notInteger };
            ++i;
            if (val <= 0 || ((opt == "EX" || opt == "EXAT") && __builtin_mul_overflow(val, static_cast<int64_t>(1000), &val)))
                return { resp::error("ERR invalid expire time in 'set' command") };
            if (opt == "EX" || opt == "PX")
            {
                if (__builtin_add_overflow(val, nowMs(), &val))
                    return { resp::error("ERR invalid expire time in 'set' command") };
            }
            expiryMs = val;
        }
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
        KeyLock lock(*this, key);

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

        int rc = 0;
        std::optional<ExpiryInfo> expiry;
        if (keepTtl)
            expiry = getExpiry(ctx.id, key, rc);
        else
            removeCurrentExpiry(ctx.id, key, rc);
        if (rc == 0 && expiryMs)
            rc = addExpiryIndex(ctx.id, key, *expiryMs, expiry.emplace());
        if (rc == 0)
            rc = setValue(ctx.id, key, command.valueArg(2), expiry);
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
            int rc = 0;
            removeCurrentExpiry(ctx.id, args[i], rc);
            if (rc == 0)
                rc = setValue(ctx.id, args[i], command.valueArg(i + 1), std::nullopt);
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
        KeyLock lock(*this, key);

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
        const auto rc = setValue(ctx.id, key, ValueArg{ .data = newValStr }, curr.expiry);
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
            bool existed = false;
            const auto rc = deleteKey(ctx.id, args[i], existed);
            if (rc != 0)
                return { resp::error(fmt::format("ERR storage error {}", rc)) };
            modified = true;

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

    return { resp::simple(keyTypeName(entry->type)) };
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
        KeyLock lock(*this, key);

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
            auto rc = prepareValue(ctx.id, key, command.valueArg(i), listContentType(), std::nullopt, prepared);
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
        KeyLock lock(*this, key);

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

    // In transactions and scripts, blocking commands don't block
    const bool canBlock = currentTx == nullptr;

    // Register before trying to pop, so pushes in between wake up the connection
    if (canBlock)
        registerWaiter(keys, conn);

    for (const auto& key : keys)
    {
        ReadRes value;
        std::string popErr;
        PopStatus res;
        {
            KeyLock lock(*this, key);
            res = popElement(ctx.id, key, left, value, popErr);
        }

        if (res == PopStatus::Error)
        {
            if (canBlock)
                unregisterWaiter(keys, conn.get());
            return { resp::error(popErr) };
        }

        if (res == PopStatus::WrongType)
        {
            if (canBlock)
                unregisterWaiter(keys, conn.get());
            return { resp::wrongType };
        }

        if (res == PopStatus::Popped)
        {
            if (canBlock)
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

    if (!canBlock)
        return { resp::nullArray(state) };

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
            else if (typeStr == "hash")
                typeFilter = KeyType::Hash;
            else if (typeStr == "set")
                typeFilter = KeyType::Set;
            else if (typeStr == "zset")
                typeFilter = KeyType::ZSet;
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

std::optional<std::string> RedisServer::deleteEntries(const int64_t bucketId, const std::string& prefix, const std::function<bool(int64_t)>& filter)
{
    syncListing();
    while (true)
    {
        // Collect a batch of entries (all versions) and delete them
        std::vector<std::string> batch;
        {
            SingleFileStorage::ListIterData iterData;
            SCOPE_EXIT { sfs.list_iter_stop(iterData); };

            const auto start = make_key(prefix, bucketId, std::numeric_limits<int64_t>::max());
            if (!sfs.list_iter_start(start, start, [bucketId, &prefix](const std::string& iterKey)
                                     {
                                         const auto keyInfo = extractKeyInfoView(iterKey);
                                         return keyInfo.bucketId == bucketId && keyInfo.key.starts_with(prefix); }, iterData))
            {
                return resp::error("ERR error listing keys");
            }

            while (batch.size() < flushBatchSize)
            {
                std::string keyBin, md5sum;
                int64_t offset, size, lastModified;
                std::vector<SingleFileStorage::SPunchItem> extraExts;
                if (!sfs.iter_curr_val(keyBin, offset, size, extraExts, lastModified, md5sum, iterData.curr))
                    break;

                const auto keyInfo = extractKeyInfoView(keyBin);
                if (keyInfo.bucketId != bucketId || !keyInfo.key.starts_with(prefix))
                    break;

                if (!filter || filter(keyInfo.version))
                    batch.push_back(std::move(keyBin));

                if (!sfs.list_iter_next(iterData))
                    return resp::error("ERR error listing keys");
            }
        }

        if (batch.empty())
            return std::nullopt;

        for (const auto& keyBin : batch)
        {
            const auto rc = sfs.del(keyBin, SingleFileStorage::DelAction::Del, false);
            if (rc != 0)
                return resp::error(fmt::format("ERR storage error {}", rc));
        }

        // Listings without WAL/manual commit only see committed deletions. Commit also in transactions/scripts, so
        // the next listing does not return the deleted entries again
        if (!(sfs.listings_include_uncommitted() ? commit() : S3Handler::commitStorage(sfs)))
            return resp::error("ERR storage commit failed");
    }
}

bool RedisServer::hasEntries(const int64_t bucketId, const std::string& prefix)
{
    SingleFileStorage::ListIterData iterData;
    SCOPE_EXIT { sfs.list_iter_stop(iterData); };

    const auto start = make_key(prefix, bucketId, std::numeric_limits<int64_t>::max());
    if (!sfs.list_iter_start(start, start, [bucketId, &prefix](const std::string& iterKey)
                             {
                                 const auto keyInfo = extractKeyInfoView(iterKey);
                                 return keyInfo.bucketId == bucketId && keyInfo.key.starts_with(prefix); }, iterData))
    {
        // Assume there are
        return true;
    }

    std::string keyBin, md5sum;
    int64_t offset, size, lastModified;
    std::vector<SingleFileStorage::SPunchItem> extraExts;
    if (!sfs.iter_curr_val(keyBin, offset, size, extraExts, lastModified, md5sum, iterData.curr))
        return false;
    const auto keyInfo = extractKeyInfoView(keyBin);
    return keyInfo.bucketId == bucketId && keyInfo.key.starts_with(prefix);
}

std::optional<std::string> RedisServer::flushReserved(const int64_t bucketId)
{
    // All entries starting with the reserved prefix in the counters bucket (expiry index, large collections)
    return deleteEntries(buckets::getCountersBucket(bucketId), reservedKeyPrefix, nullptr);
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

    if (auto err = deleteEntries(ctx.id, std::string(), nullptr))
        return { *err };
    if (auto err = flushReserved(ctx.id))
        return { *err };

    return { resp::ok };
}

RedisServer::ExecResult RedisServer::cmdSetex(ConnState& state, const RedisCommand& command, const bool ms)
{
    const auto& args = command.args;
    if (args.size() != 4)
        return { resp::wrongArgs(ms ? "psetex" : "setex") };

    // SET key value PX/EX ttl
    RedisCommand setCmd{ .args = { "SET", args[1], args[3], ms ? "PX" : "EX", args[2] } };
    if (auto streamed = command.streamedArg(3))
        setCmd.streamed = { nullptr, nullptr, streamed, nullptr, nullptr };
    return cmdSet(state, setCmd);
}

RedisServer::ExecResult RedisServer::cmdGetdel(ConnState& state, const std::vector<std::string>& args)
{
    if (args.size() != 2)
        return { resp::wrongArgs("getdel") };

    const auto& key = args[1];
    if (auto err = checkKey(key))
        return { *err };
    if (auto err = checkAuth(state, args[0], { Action::GetObject, Action::DeleteObject }, key))
        return { *err };
    BucketCtx ctx;
    if (auto err = getBucketCtx(ctx))
        return { *err };

    ExecResult reply;
    {
        KeyLock lock(*this, key);
        auto res = getString(key, ReadMode::Bulk);
        if (res.status == ReadRes::Status::NotFound)
            return { resp::nullBulk(state) };
        if (res.status == ReadRes::Status::WrongType)
            return { resp::wrongType };
        if (res.status == ReadRes::Status::Error)
            return { resp::error(fmt::format("ERR {}", res.value)) };

        bool existed;
        const auto rc = deleteKey(ctx.id, key, existed);
        if (rc != 0)
            return { resp::error(fmt::format("ERR storage error {}", rc)) };

        ReplyBuilder builder;
        builder.appendBulk(res);
        reply = builder.finish();
    }

    if (!commit())
        return { resp::error("ERR storage commit failed") };
    return reply;
}

RedisServer::ExecResult RedisServer::cmdExpire(ConnState& state, const std::vector<std::string>& args, const std::string& cmd)
{
    if (args.size() < 3)
        return { resp::wrongArgs(toLower(cmd)) };

    int64_t val;
    if (!parseInt64(args[2], val))
        return { resp::notInteger };

    bool nx = false, xx = false, gt = false, lt = false;
    for (size_t i = 3; i < args.size(); ++i)
    {
        const auto opt = toUpper(args[i]);
        if (opt == "NX")
            nx = true;
        else if (opt == "XX")
            xx = true;
        else if (opt == "GT")
            gt = true;
        else if (opt == "LT")
            lt = true;
        else
            return { resp::error(fmt::format("ERR Unsupported option {}", args[i])) };
    }
    if (nx && (xx || gt || lt))
        return { resp::error("ERR NX and XX, GT or LT options at the same time are not compatible") };
    if (gt && lt)
        return { resp::error("ERR GT and LT options at the same time are not compatible") };

    int64_t expiryMs;
    const auto now = nowMs();
    const bool relative = cmd == "EXPIRE" || cmd == "PEXPIRE";
    const bool seconds = cmd == "EXPIRE" || cmd == "EXPIREAT";
    if (seconds && __builtin_mul_overflow(val, static_cast<int64_t>(1000), &val))
        return { resp::error(fmt::format("ERR invalid expire time in '{}' command", toLower(cmd))) };
    if (relative)
    {
        if (__builtin_add_overflow(val, now, &expiryMs))
            return { resp::error(fmt::format("ERR invalid expire time in '{}' command", toLower(cmd))) };
    }
    else
    {
        expiryMs = val;
    }

    const auto& key = args[1];
    if (auto err = checkKey(key))
        return { *err };
    if (auto err = checkAuth(state, args[0], { Action::GetObject, Action::PutObject }, key))
        return { *err };
    BucketCtx ctx;
    if (auto err = getBucketCtx(ctx))
        return { *err };

    {
        KeyLock lock(*this, key);
        int err = 0;
        const auto entry = findElement(ctx.id, key, true, err);
        if (err != 0)
            return { resp::error(fmt::format("ERR storage error {}", err)) };
        if (!entry)
            return { resp::integer(0) };

        const auto curr = getExpiry(ctx.id, key, err);
        if (err != 0)
            return { resp::error(fmt::format("ERR storage error {}", err)) };

        // No expiry counts as infinite TTL for GT/LT
        if ((nx && curr) || (xx && !curr) || (gt && (!curr || expiryMs <= curr->ms)) || (lt && curr && expiryMs >= curr->ms))
            return { resp::integer(0) };

        int rc;
        if (expiryMs <= now)
        {
            bool existed;
            rc = deleteKey(ctx.id, key, existed);
        }
        else
        {
            rc = setExpiry(ctx.id, key, expiryMs);
        }
        if (rc != 0)
            return { resp::error(fmt::format("ERR storage error {}", rc)) };
    }

    if (!commit())
        return { resp::error("ERR storage commit failed") };

    return { resp::integer(1) };
}

RedisServer::ExecResult RedisServer::cmdTtl(ConnState& state, const std::vector<std::string>& args, const std::string& cmd)
{
    if (args.size() != 2)
        return { resp::wrongArgs(toLower(cmd)) };

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
    if (!entry)
        return { resp::integer(-2) };

    const auto expiry = getExpiry(ctx.id, key, err);
    if (err != 0)
        return { resp::error(fmt::format("ERR storage error {}", err)) };
    if (!expiry)
        return { resp::integer(-1) };

    if (cmd == "EXPIRETIME")
        return { resp::integer((expiry->ms + 500) / 1000) };
    if (cmd == "PEXPIRETIME")
        return { resp::integer(expiry->ms) };

    const auto remaining = std::max(expiry->ms - nowMs(), static_cast<int64_t>(0));
    if (cmd == "PTTL")
        return { resp::integer(remaining) };
    return { resp::integer((remaining + 500) / 1000) };
}

RedisServer::ExecResult RedisServer::cmdPersist(ConnState& state, const std::vector<std::string>& args)
{
    if (args.size() != 2)
        return { resp::wrongArgs("persist") };

    const auto& key = args[1];
    if (auto err = checkKey(key))
        return { *err };
    if (auto err = checkAuth(state, args[0], { Action::GetObject, Action::PutObject }, key))
        return { *err };
    BucketCtx ctx;
    if (auto err = getBucketCtx(ctx))
        return { *err };

    bool removed = false;
    {
        KeyLock lock(*this, key);
        const auto rc = clearExpiry(ctx.id, key, &removed);
        if (rc != 0)
            return { resp::error(fmt::format("ERR storage error {}", rc)) };
    }

    if (removed && !commit())
        return { resp::error("ERR storage commit failed") };
    return { resp::integer(removed ? 1 : 0) };
}

namespace
{
    // Commands modifying collections. Removing commands need the DeleteObject permission as well
    bool isCollectionWrite(const std::string& cmd)
    {
        static const std::unordered_set<std::string_view> cmds = { "HSET", "HMSET", "HSETNX", "HDEL", "HINCRBY", "SADD", "SREM", "ZADD",
                                                                   "ZREM", "ZINCRBY", "ZREMRANGEBYSCORE", "ZREMRANGEBYRANK" };
        return cmds.contains(cmd);
    }

    bool isCollectionRemove(const std::string& cmd)
    {
        return cmd == "HDEL" || cmd == "SREM" || cmd == "ZREM" || cmd == "ZREMRANGEBYSCORE" || cmd == "ZREMRANGEBYRANK";
    }

    // Normalizes start/stop indices like LRANGE. Returns false if the range is empty
    bool normalizeRange(int64_t& start, int64_t& stop, const int64_t len)
    {
        if (start < 0)
            start += len;
        if (stop < 0)
            stop += len;
        if (start < 0)
            start = 0;
        if (stop >= len)
            stop = len - 1;
        return start <= stop && start < len;
    }

    std::string storageError(const int rc)
    {
        return resp::error(fmt::format("ERR storage error {}", rc));
    }
} // namespace

std::optional<std::string> RedisServer::checkCollectionAuth(ConnState& state, const std::vector<std::string>& args, const std::string& cmd)
{
    const auto& key = args[1];
    if (auto err = checkKey(key))
        return err;
    if (auto err = checkAuth(state, args[0], { Action::GetObject }, key))
        return err;
    if (isCollectionWrite(cmd))
    {
        if (auto err = checkAuth(state, args[0], { Action::PutObject }, key))
            return err;
    }
    if (isCollectionRemove(cmd))
    {
        if (auto err = checkAuth(state, args[0], { Action::DeleteObject }, key))
            return err;
    }
    return std::nullopt;
}

std::unique_ptr<RedisServer::CollOps> RedisServer::collOps(const int64_t bucketId, const std::string& key, const KeyType type, CollState& cs)
{
    if (cs.large)
        return std::make_unique<LargeCollOps>(*this, bucketId, key, type);
    return std::make_unique<SmallCollOps>(cs.small);
}

RedisServer::ExecResult RedisServer::finishCollectionWrite(const int64_t bucketId, const std::string& key, const KeyType type, CollState& cs,
                                                           CollOps& ops, CollLocks& locks, const bool modified, const bool removed, ExecResult reply)
{
    bool converted = false;
    if (!cs.large)
    {
        if (static_cast<SmallCollOps&>(ops).dirty)
        {
            const auto rc = writeSmallCollection(bucketId, key, cs.small, converted);
            if (rc != 0)
                return { storageError(rc) };
        }
    }

    // Listings (without WAL/manual commit) only see committed entries. Commit modified entries of large collections
    // before releasing the locks, so reads of multiple elements are atomic. Other modifications are visible to reads
    // before committing
    const bool commitLocked = modified && (cs.large || converted) && !sfs.listings_include_uncommitted();
    if (commitLocked && !commit())
        return { resp::error("ERR storage commit failed") };

    locks.fields.reset();
    locks.group.reset();
    locks.shared.reset();
    locks.exclusive.reset();

    if (modified && !commitLocked && !commit())
        return { resp::error("ERR storage commit failed") };

    // Delete large collections without elements
    if (cs.large && removed)
        deleteIfEmpty(bucketId, key, type);

    return reply;
}

RedisServer::ExecResult RedisServer::cmdHash(ConnState& state, const std::vector<std::string>& args, const std::string& cmd)
{
    const auto argc = args.size();
    const auto lcmd = toLower(cmd);
    if (argc < 2)
        return { resp::wrongArgs(lcmd) };

    if ((cmd == "HSET" || cmd == "HMSET") && (argc < 4 || argc % 2 != 0))
        return { resp::wrongArgs(lcmd) };
    if ((cmd == "HSETNX" || cmd == "HINCRBY") && argc != 4)
        return { resp::wrongArgs(lcmd) };
    if ((cmd == "HGET" || cmd == "HEXISTS" || cmd == "HSTRLEN") && argc != 3)
        return { resp::wrongArgs(lcmd) };
    if ((cmd == "HMGET" || cmd == "HDEL") && argc < 3)
        return { resp::wrongArgs(lcmd) };
    if ((cmd == "HGETALL" || cmd == "HLEN" || cmd == "HKEYS" || cmd == "HVALS") && argc != 2)
        return { resp::wrongArgs(lcmd) };

    int64_t increment = 0;
    if (cmd == "HINCRBY" && !parseInt64(args[3], increment))
        return { resp::notInteger };

    const auto& key = args[1];
    if (auto err = checkCollectionAuth(state, args, cmd))
        return { *err };
    BucketCtx ctx;
    if (auto err = getBucketCtx(ctx))
        return { *err };

    const bool write = isCollectionWrite(cmd);
    CollLocks locks;
    CollState cs;
    if (write)
    {
        std::vector<std::string> fields;
        for (size_t i = 2; i < argc; i += (cmd == "HSET" || cmd == "HMSET") ? 2 : 1)
        {
            fields.push_back(args[i]);
            if (cmd == "HSETNX" || cmd == "HINCRBY")
                break;
        }
        std::optional<std::string> err;
        if (!locksCollection(key, KeyType::Hash, fields, locks, cs, err))
            return { *err };
    }
    else if (auto err = openCollectionForRead(key, KeyType::Hash, cmd == "HGETALL" || cmd == "HKEYS" || cmd == "HVALS" || cmd == "HMGET", locks, cs))
    {
        return { *err };
    }

    auto ops = collOps(ctx.id, key, KeyType::Hash, cs);
    int err = 0;

    if (!write)
    {
        if (cmd == "HGET")
        {
            const auto val = ops->get(args[2], err);
            if (err != 0)
                return { storageError(err) };
            return { val ? resp::bulk(*val) : resp::nullBulk(state) };
        }
        if (cmd == "HEXISTS")
        {
            const auto exists = ops->contains(args[2], err);
            if (err != 0)
                return { storageError(err) };
            return { resp::integer(exists ? 1 : 0) };
        }
        if (cmd == "HSTRLEN")
        {
            const auto val = ops->get(args[2], err);
            if (err != 0)
                return { storageError(err) };
            return { resp::integer(val ? static_cast<int64_t>(val->size()) : 0) };
        }
        if (cmd == "HLEN")
        {
            const auto len = ops->count(err);
            if (err != 0)
                return { storageError(err) };
            return { resp::integer(len) };
        }
        if (cmd == "HMGET")
        {
            std::string reply = resp::arrayHeader(argc - 2);
            for (size_t i = 2; i < argc; ++i)
            {
                const auto val = ops->get(args[i], err);
                if (err != 0)
                    return { storageError(err) };
                if (val)
                    resp::appendBulk(reply, *val);
                else
                    reply += resp::nullBulk(state);
            }
            return { reply };
        }

        // HGETALL, HKEYS, HVALS
        std::string elems;
        size_t num = 0;
        if (!ops->forEach([&](const std::string& field, const std::string& value)
                          {
                              if (cmd != "HVALS")
                                  resp::appendBulk(elems, field);
                              if (cmd != "HKEYS")
                                  resp::appendBulk(elems, value);
                              ++num;
                              return true; }))
        {
            return { resp::error("ERR error listing hash fields") };
        }
        std::string reply;
        if (cmd == "HGETALL")
            reply = state.resp3 ? fmt::format("%{}\r\n", num) : resp::arrayHeader(num * 2);
        else
            reply = resp::arrayHeader(num);
        return { reply + elems };
    }

    std::string reply;
    bool modified = false, removed = false;
    if (cmd == "HSET" || cmd == "HMSET")
    {
        int64_t added = 0;
        for (size_t i = 2; i + 1 < argc; i += 2)
        {
            bool fieldAdded;
            const auto rc = ops->set(args[i], args[i + 1], fieldAdded);
            if (rc != 0)
                return { storageError(rc) };
            if (fieldAdded)
                ++added;
        }
        modified = true;
        reply = cmd == "HSET" ? resp::integer(added) : resp::ok;
    }
    else if (cmd == "HSETNX")
    {
        const auto exists = ops->contains(args[2], err);
        if (err != 0)
            return { storageError(err) };
        if (!exists)
        {
            bool added;
            const auto rc = ops->set(args[2], args[3], added);
            if (rc != 0)
                return { storageError(rc) };
            modified = true;
        }
        reply = resp::integer(exists ? 0 : 1);
    }
    else if (cmd == "HDEL")
    {
        int64_t num = 0;
        for (size_t i = 2; i < argc; ++i)
        {
            bool fieldRemoved;
            const auto rc = ops->remove(args[i], fieldRemoved);
            if (rc != 0)
                return { storageError(rc) };
            if (fieldRemoved)
                ++num;
        }
        modified = removed = num > 0;
        reply = resp::integer(num);
    }
    else if (cmd == "HINCRBY")
    {
        const auto val = ops->get(args[2], err);
        if (err != 0)
            return { storageError(err) };
        int64_t curr = 0;
        if (val && !parseInt64(*val, curr))
            return { resp::error("ERR hash value is not an integer") };
        int64_t newVal;
        if (__builtin_add_overflow(curr, increment, &newVal))
            return { resp::error("ERR increment or decrement would overflow") };
        bool added;
        const auto rc = ops->set(args[2], std::to_string(newVal), added);
        if (rc != 0)
            return { storageError(rc) };
        modified = true;
        reply = resp::integer(newVal);
    }

    return finishCollectionWrite(ctx.id, key, KeyType::Hash, cs, *ops, locks, modified, removed, { reply });
}

RedisServer::ExecResult RedisServer::cmdSetType(ConnState& state, const std::vector<std::string>& args, const std::string& cmd)
{
    const auto argc = args.size();
    const auto lcmd = toLower(cmd);
    if ((cmd == "SADD" || cmd == "SREM" || cmd == "SMISMEMBER") && argc < 3)
        return { resp::wrongArgs(lcmd) };
    if (cmd == "SISMEMBER" && argc != 3)
        return { resp::wrongArgs(lcmd) };
    if ((cmd == "SMEMBERS" || cmd == "SCARD") && argc != 2)
        return { resp::wrongArgs(lcmd) };

    const auto& key = args[1];
    if (auto err = checkCollectionAuth(state, args, cmd))
        return { *err };
    BucketCtx ctx;
    if (auto err = getBucketCtx(ctx))
        return { *err };

    const bool write = isCollectionWrite(cmd);
    CollLocks locks;
    CollState cs;
    if (write)
    {
        std::optional<std::string> err;
        if (!locksCollection(key, KeyType::Set, std::vector<std::string>(args.begin() + 2, args.end()), locks, cs, err))
            return { *err };
    }
    else if (auto err = openCollectionForRead(key, KeyType::Set, cmd == "SMEMBERS" || cmd == "SMISMEMBER", locks, cs))
    {
        return { *err };
    }

    auto ops = collOps(ctx.id, key, KeyType::Set, cs);
    int err = 0;

    if (cmd == "SMEMBERS")
    {
        std::string elems;
        size_t num = 0;
        if (!ops->forEach([&](const std::string& member, const std::string& /*value*/)
                          {
                              resp::appendBulk(elems, member);
                              ++num;
                              return true; }))
        {
            return { resp::error("ERR error listing set members") };
        }
        return { (state.resp3 ? fmt::format("~{}\r\n", num) : resp::arrayHeader(num)) + elems };
    }
    if (cmd == "SCARD")
    {
        const auto num = ops->count(err);
        if (err != 0)
            return { storageError(err) };
        return { resp::integer(num) };
    }
    if (cmd == "SISMEMBER" || cmd == "SMISMEMBER")
    {
        std::string reply = cmd == "SMISMEMBER" ? resp::arrayHeader(argc - 2) : std::string();
        for (size_t i = 2; i < argc; ++i)
        {
            const auto exists = ops->contains(args[i], err);
            if (err != 0)
                return { storageError(err) };
            reply += resp::integer(exists ? 1 : 0);
        }
        return { reply };
    }

    int64_t changed = 0;
    for (size_t i = 2; i < argc; ++i)
    {
        bool memberChanged;
        const auto rc = cmd == "SADD" ? ops->set(args[i], std::string(), memberChanged) : ops->remove(args[i], memberChanged);
        if (rc != 0)
            return { storageError(rc) };
        if (memberChanged)
            ++changed;
    }

    return finishCollectionWrite(ctx.id, key, KeyType::Set, cs, *ops, locks, changed > 0, cmd == "SREM" && changed > 0, { resp::integer(changed) });
}

RedisServer::ExecResult RedisServer::cmdZset(ConnState& state, const std::vector<std::string>& args, const std::string& cmd)
{
    const auto argc = args.size();
    const auto lcmd = toLower(cmd);
    if (argc < 2)
        return { resp::wrongArgs(lcmd) };

    const auto doubleReply = [&state](const double val)
    {
        if (state.resp3)
            return fmt::format(",{}\r\n", formatDouble(val));
        return resp::bulk(formatDouble(val));
    };

    const auto& key = args[1];
    if (auto err = checkCollectionAuth(state, args, cmd))
        return { *err };
    BucketCtx ctx;
    if (auto err = getBucketCtx(ctx))
        return { *err };

    // Parse arguments before reading the value
    bool nx = false, xx = false, gt = false, lt = false, ch = false, incr = false;
    std::vector<std::pair<double, std::string>> addPairs;
    if (cmd == "ZADD")
    {
        size_t i = 2;
        for (; i < argc; ++i)
        {
            const auto opt = toUpper(args[i]);
            if (opt == "NX")
                nx = true;
            else if (opt == "XX")
                xx = true;
            else if (opt == "GT")
                gt = true;
            else if (opt == "LT")
                lt = true;
            else if (opt == "CH")
                ch = true;
            else if (opt == "INCR")
                incr = true;
            else
                break;
        }
        if (i >= argc || (argc - i) % 2 != 0)
            return { resp::syntaxError };
        if (nx && xx)
            return { resp::error("ERR XX and NX options at the same time are not compatible") };
        if ((gt && nx) || (lt && nx) || (gt && lt))
            return { resp::error("ERR GT, LT, and/or NX options at the same time are not compatible") };
        if (incr && argc - i != 2)
            return { resp::error("ERR INCR option supports a single increment-element pair") };
        for (; i + 1 < argc; i += 2)
        {
            double score;
            if (!parseDouble(args[i], score))
                return { resp::error("ERR value is not a valid float") };
            addPairs.emplace_back(score, args[i + 1]);
        }
    }
    else if (cmd == "ZINCRBY")
    {
        if (argc != 4)
            return { resp::wrongArgs(lcmd) };
        double score;
        if (!parseDouble(args[2], score))
            return { resp::error("ERR value is not a valid float") };
        addPairs.emplace_back(score, args[3]);
        incr = true;
    }
    else if ((cmd == "ZREM" || cmd == "ZMSCORE") && argc < 3)
        return { resp::wrongArgs(lcmd) };
    else if (cmd == "ZCARD" && argc != 2)
        return { resp::wrongArgs(lcmd) };
    else if ((cmd == "ZSCORE" || cmd == "ZRANK" || cmd == "ZREVRANK") && argc != 3)
        return { resp::wrongArgs(lcmd) };
    else if ((cmd == "ZCOUNT" || cmd == "ZREMRANGEBYSCORE" || cmd == "ZREMRANGEBYRANK") && argc != 4)
        return { resp::wrongArgs(lcmd) };

    // Range queries
    bool byScore = false, rev = false, withScores = false, hasLimit = false;
    int64_t offset = 0, limitCount = -1;
    ScoreBound minBound{ .val = -std::numeric_limits<double>::infinity(), .exclusive = false };
    ScoreBound maxBound{ .val = std::numeric_limits<double>::infinity(), .exclusive = false };
    int64_t start = 0, stop = -1;
    const bool isRange = cmd == "ZRANGE" || cmd == "ZREVRANGE" || cmd == "ZRANGEBYSCORE" || cmd == "ZREVRANGEBYSCORE";
    if (isRange)
    {
        if (argc < 4)
            return { resp::wrongArgs(lcmd) };
        byScore = cmd == "ZRANGEBYSCORE" || cmd == "ZREVRANGEBYSCORE";
        rev = cmd == "ZREVRANGE" || cmd == "ZREVRANGEBYSCORE";
        for (size_t i = 4; i < argc; ++i)
        {
            const auto opt = toUpper(args[i]);
            if (opt == "WITHSCORES")
                withScores = true;
            else if (opt == "BYSCORE" && cmd == "ZRANGE")
                byScore = true;
            else if (opt == "REV" && cmd == "ZRANGE")
                rev = true;
            else if (opt == "BYLEX" && cmd == "ZRANGE")
                return { resp::error("ERR BYLEX is not supported") };
            else if (opt == "LIMIT" && cmd != "ZREVRANGE" && i + 2 < argc)
            {
                if (!parseInt64(args[i + 1], offset) || !parseInt64(args[i + 2], limitCount))
                    return { resp::notInteger };
                hasLimit = true;
                i += 2;
            }
            else
                return { resp::syntaxError };
        }
        if (hasLimit && !byScore)
            return { resp::error("ERR syntax error, LIMIT is only supported in combination with either BYSCORE or BYLEX") };

        if (byScore)
        {
            // ZRANGE ... BYSCORE REV and ZREVRANGEBYSCORE take max first
            const auto& minArg = rev ? args[3] : args[2];
            const auto& maxArg = rev ? args[2] : args[3];
            if (!parseScoreBound(minArg, minBound) || !parseScoreBound(maxArg, maxBound))
                return { resp::error("ERR min or max is not a float") };
        }
        else if (!parseInt64(args[2], start) || !parseInt64(args[3], stop))
            return { resp::notInteger };
    }
    else if (cmd == "ZCOUNT" || cmd == "ZREMRANGEBYSCORE")
    {
        if (!parseScoreBound(args[2], minBound) || !parseScoreBound(args[3], maxBound))
            return { resp::error("ERR min or max is not a float") };
    }
    else if (cmd == "ZREMRANGEBYRANK")
    {
        if (!parseInt64(args[2], start) || !parseInt64(args[3], stop))
            return { resp::notInteger };
    }

    const bool write = isCollectionWrite(cmd);
    CollLocks locks;
    CollState cs;
    if (write)
    {
        // Range removals lock the whole key
        std::vector<std::string> fields;
        if (cmd == "ZREM")
            fields.assign(args.begin() + 2, args.end());
        for (const auto& [score, member] : addPairs)
            fields.push_back(member);
        std::optional<std::string> err;
        if (!locksCollection(key, KeyType::ZSet, fields, locks, cs, err))
            return { *err };
    }
    else if (auto err = openCollectionForRead(key, KeyType::ZSet, cmd != "ZSCORE" && cmd != "ZCARD", locks, cs))
    {
        return { *err };
    }

    auto ops = collOps(ctx.id, key, KeyType::ZSet, cs);
    int err = 0;

    // Elements with score in [minBound, maxBound] in ascending order. f returns false to stop
    const auto forEachInRange = [&](const std::function<bool(double, const std::string&)>& f)
    {
        return ops->forEachByScore(std::isinf(minBound.val) && minBound.val < 0 ? std::nullopt : std::optional<double>(minBound.val),
                                   [&](const double score, const std::string& member)
                                   {
                                       if (!belowMax(score, maxBound))
                                           return false;
                                       if (!aboveMin(score, minBound))
                                           return true;
                                       return f(score, member);
                                   });
    };

    if (cmd == "ZCARD")
    {
        const auto num = ops->count(err);
        if (err != 0)
            return { storageError(err) };
        return { resp::integer(num) };
    }
    if (cmd == "ZSCORE" || cmd == "ZMSCORE")
    {
        std::string reply = cmd == "ZMSCORE" ? resp::arrayHeader(argc - 2) : std::string();
        for (size_t i = 2; i < argc; ++i)
        {
            const auto score = ops->zscore(args[i], err);
            if (err != 0)
                return { storageError(err) };
            reply += score ? doubleReply(*score) : resp::nullBulk(state);
        }
        return { reply };
    }
    if (cmd == "ZRANK" || cmd == "ZREVRANK")
    {
        const auto score = ops->zscore(args[2], err);
        if (err != 0)
            return { storageError(err) };
        if (!score)
            return { resp::nullBulk(state) };
        int64_t rank = 0, total = 0;
        bool found = false;
        if (!ops->forEachByScore(std::nullopt, [&](const double /*score*/, const std::string& member)
                                 {
                                     if (member == args[2])
                                         found = true;
                                     if (!found)
                                         ++rank;
                                     ++total;
                                     return !found || cmd == "ZREVRANK"; }))
        {
            return { resp::error("ERR error listing sorted set") };
        }
        if (!found)
            return { resp::nullBulk(state) };
        return { resp::integer(cmd == "ZRANK" ? rank : total - 1 - rank) };
    }
    if (cmd == "ZCOUNT")
    {
        int64_t num = 0;
        if (!forEachInRange([&](double, const std::string&)
                            {
                                ++num;
                                return true; }))
        {
            return { resp::error("ERR error listing sorted set") };
        }
        return { resp::integer(num) };
    }
    if (isRange)
    {
        std::vector<std::pair<double, std::string>> elems;
        bool ok;
        if (byScore)
        {
            // Without REV, only the elements up to offset + count are needed
            const auto maxElems = !rev && hasLimit && limitCount >= 0 && offset >= 0 ? offset + limitCount : std::numeric_limits<int64_t>::max();
            ok = forEachInRange([&](const double score, const std::string& member)
                                {
                                    if (static_cast<int64_t>(elems.size()) >= maxElems)
                                        return false;
                                    elems.emplace_back(score, member);
                                    return true; });
            if (rev)
                std::reverse(elems.begin(), elems.end());
            if (hasLimit)
            {
                if (offset < 0 || offset >= static_cast<int64_t>(elems.size()))
                    elems.clear();
                else
                {
                    elems.erase(elems.begin(), elems.begin() + offset);
                    if (limitCount >= 0 && limitCount < static_cast<int64_t>(elems.size()))
                        elems.resize(static_cast<size_t>(limitCount));
                }
            }
        }
        else
        {
            // Without REV and with non-negative indices, only the elements up to stop are needed
            const auto maxElems = !rev && start >= 0 && stop >= 0 ? stop + 1 : std::numeric_limits<int64_t>::max();
            ok = ops->forEachByScore(std::nullopt, [&](const double score, const std::string& member)
                                     {
                                         if (static_cast<int64_t>(elems.size()) >= maxElems)
                                             return false;
                                         elems.emplace_back(score, member);
                                         return true; });
            if (rev)
                std::reverse(elems.begin(), elems.end());
            const auto len = maxElems == std::numeric_limits<int64_t>::max() ? static_cast<int64_t>(elems.size()) : ops->count(err);
            if (!normalizeRange(start, stop, len) || start >= static_cast<int64_t>(elems.size()))
                elems.clear();
            else
                elems = std::vector<std::pair<double, std::string>>(elems.begin() + start,
                                                                    elems.begin() + std::min(stop + 1, static_cast<int64_t>(elems.size())));
        }
        if (!ok)
            return { resp::error("ERR error listing sorted set") };

        std::string reply = resp::arrayHeader(withScores && !state.resp3 ? elems.size() * 2 : elems.size());
        for (const auto& [score, member] : elems)
        {
            if (withScores && state.resp3)
                reply += resp::arrayHeader(2);
            resp::appendBulk(reply, member);
            if (withScores)
                reply += doubleReply(score);
        }
        return { reply };
    }

    // Modifications
    std::string reply;
    bool modified = false, removed = false;
    if (cmd == "ZADD" || cmd == "ZINCRBY")
    {
        int64_t added = 0, changed = 0;
        std::optional<double> incrResult;
        for (const auto& [score, member] : addPairs)
        {
            const auto curr = ops->zscore(member, err);
            if (err != 0)
                return { storageError(err) };
            if ((nx && curr) || (xx && !curr))
                continue;
            double newScore = score;
            if (incr && curr)
            {
                newScore = *curr + score;
                if (std::isnan(newScore))
                    return { resp::error("ERR resulting score is not a number (NaN)") };
            }
            if (curr && ((gt && newScore <= *curr) || (lt && newScore >= *curr)))
                continue;
            incrResult = newScore;
            if (curr && newScore == *curr)
                continue;
            bool memberAdded;
            const auto rc = ops->zset(member, newScore, memberAdded);
            if (rc != 0)
                return { storageError(rc) };
            modified = true;
            if (memberAdded)
                ++added;
            else
                ++changed;
        }

        if (incr)
            reply = incrResult ? doubleReply(*incrResult) : resp::nullBulk(state);
        else
            reply = resp::integer(ch ? added + changed : added);
    }
    else if (cmd == "ZREM" || cmd == "ZREMRANGEBYSCORE" || cmd == "ZREMRANGEBYRANK")
    {
        std::vector<std::string> toRemove;
        if (cmd == "ZREM")
        {
            toRemove.assign(args.begin() + 2, args.end());
        }
        else if (cmd == "ZREMRANGEBYSCORE")
        {
            if (!forEachInRange([&](double, const std::string& member)
                                {
                                    toRemove.push_back(member);
                                    return true; }))
            {
                return { resp::error("ERR error listing sorted set") };
            }
        }
        else
        {
            const auto len = ops->count(err);
            if (err != 0)
                return { storageError(err) };
            if (normalizeRange(start, stop, len))
            {
                int64_t idx = 0;
                if (!ops->forEachByScore(std::nullopt, [&](double, const std::string& member)
                                         {
                                             if (idx >= start)
                                                 toRemove.push_back(member);
                                             return ++idx <= stop; }))
                {
                    return { resp::error("ERR error listing sorted set") };
                }
            }
        }

        int64_t num = 0;
        for (const auto& member : toRemove)
        {
            bool memberRemoved;
            const auto rc = ops->remove(member, memberRemoved);
            if (rc != 0)
                return { storageError(rc) };
            if (memberRemoved)
                ++num;
        }
        modified = removed = num > 0;
        reply = resp::integer(num);
    }
    else
    {
        return { resp::error("ERR unknown sorted set command") };
    }

    return finishCollectionWrite(ctx.id, key, KeyType::ZSet, cs, *ops, locks, modified, removed, { reply });
}

RedisServer::ExecResult RedisServer::cmdPublish(ConnState& state, const std::vector<std::string>& args)
{
    if (args.size() != 3)
        return { resp::wrongArgs("publish") };
    // Publishing needs the permission to write the bucket
    if (auto err = checkAuth(state, args[0], { Action::PutObject }, args[1]))
        return { *err };
    return { resp::integer(publish(args[1], args[2])) };
}

RedisServer::ExecResult RedisServer::cmdPubsub(ConnState& state, const std::vector<std::string>& args, const std::string& cmd,
                                               const std::shared_ptr<RedisConnection>& conn)
{
    const bool pattern = cmd == "PSUBSCRIBE" || cmd == "PUNSUBSCRIBE";
    const bool sub = cmd == "SUBSCRIBE" || cmd == "PSUBSCRIBE";
    const auto lcmd = toLower(cmd);
    if (sub && args.size() < 2)
        return { resp::wrongArgs(lcmd) };
    if (currentTx != nullptr)
        return { resp::error(fmt::format("ERR {} is not allowed in transactions or scripts", lcmd)) };

    auto& names = pattern ? state.patterns : state.channels;
    const auto header = state.resp3 ? std::string(">3\r\n") : resp::arrayHeader(3);
    const auto count = [&state]()
    {
        return static_cast<int64_t>(state.channels.size() + state.patterns.size());
    };

    std::string reply;
    if (sub)
    {
        for (size_t i = 1; i < args.size(); ++i)
        {
            const auto& name = args[i];
            // Subscribing needs read permission on the channel (as if it was a key)
            if (auto err = checkAuth(state, args[0], { Action::GetObject }, name))
            {
                reply += *err;
                continue;
            }
            if (names.insert(name).second)
                subscribe(name, pattern, state.resp3, conn);
            reply += header;
            resp::appendBulk(reply, lcmd);
            resp::appendBulk(reply, name);
            reply += resp::integer(count());
        }
        return { reply };
    }

    std::vector<std::string> toRemove;
    if (args.size() == 1)
        toRemove.assign(names.begin(), names.end());
    else
        toRemove.assign(args.begin() + 1, args.end());

    if (toRemove.empty())
    {
        reply += header;
        resp::appendBulk(reply, lcmd);
        reply += resp::nullBulk(state);
        reply += resp::integer(count());
        return { reply };
    }

    for (const auto& name : toRemove)
    {
        if (names.erase(name) > 0)
            unsubscribe(name, pattern, conn.get());
        reply += header;
        resp::appendBulk(reply, lcmd);
        resp::appendBulk(reply, name);
        reply += resp::integer(count());
    }
    return { reply };
}

namespace
{
    // Script running on this thread (for redis.call)
    struct ScriptCtx
    {
        RedisServer* server;
        RedisServer::ConnState* state;
        const std::shared_ptr<RedisConnection>* conn;
        std::chrono::steady_clock::time_point deadline;
        std::string sha;
        // Keys declared by the script (KEYS)
        std::unordered_set<std::string> declaredKeys;
        // Script can be aborted and run again with the global lock
        bool canRetry = false;
        // Script modified something (so it cannot be run again)
        bool wrote = false;
        // Script accessed an undeclared key and has to be run again with the global lock
        bool needsGlobalLock = false;
    };

    // Commands accessing all keys
    bool accessesAllKeys(const std::string& cmd)
    {
        return cmd == "SCAN" || cmd == "KEYS" || cmd == "DBSIZE" || cmd == "FLUSHDB" || cmd == "FLUSHALL";
    }

    thread_local ScriptCtx* currentScript = nullptr;

    // Lua state per thread, with compiled scripts (by SHA1) in the registry
    struct LuaStateHolder
    {
        lua_State* L = nullptr;

        ~LuaStateHolder()
        {
            if (L != nullptr)
                lua_close(L);
        }
    };

    thread_local LuaStateHolder luaHolder;

    constexpr const char* scriptsRegistryKey = "hs5_scripts";

    // Reads the complete reply of a command (including values streamed from the data file)
    std::string materializeReply(RedisServer::ExecResult& res)
    {
        std::string ret = std::move(res.reply);
        for (auto& part : res.streamParts)
        {
            ret += part.prefix;
            const auto size = part.reader->FileSize();
            const auto offset = ret.size();
            ret.resize(offset + static_cast<size_t>(size));
            if (size > 0)
                part.reader->Read(ret.data() + offset, size, 0);
        }
        ret += res.tail;
        return ret;
    }

    void luaTimeoutHook(lua_State* L, lua_Debug* /*ar*/)
    {
        if (currentScript != nullptr && std::chrono::steady_clock::now() > currentScript->deadline)
            luaL_error(L, "ERR script timeout");
    }
} // namespace

// C functions of the redis table in Lua scripts
class RedisServer::LuaScript
{
public:
    static int call(lua_State* L)
    {
        return callImpl(L, false);
    }

    static int pcall(lua_State* L)
    {
        return callImpl(L, true);
    }

    static int errorReply(lua_State* L)
    {
        const char* msg = luaL_checkstring(L, 1);
        lua_newtable(L);
        lua_pushstring(L, msg);
        lua_setfield(L, -2, "err");
        return 1;
    }

    static int statusReply(lua_State* L)
    {
        const char* msg = luaL_checkstring(L, 1);
        lua_newtable(L);
        lua_pushstring(L, msg);
        lua_setfield(L, -2, "ok");
        return 1;
    }

    static int sha1hex(lua_State* L)
    {
        size_t len;
        const char* str = luaL_checklstring(L, 1, &len);
        const auto hex = sha1Hex(std::string_view(str, len));
        lua_pushlstring(L, hex.data(), hex.size());
        return 1;
    }

    static int noop(lua_State* /*L*/)
    {
        return 0;
    }

    static lua_State* getState()
    {
        if (luaHolder.L != nullptr)
            return luaHolder.L;

        lua_State* L = luaL_newstate();
        if (L == nullptr)
            return nullptr;

        // Only safe libraries (no io, os, package, debug)
        luaL_requiref(L, LUA_GNAME, luaopen_base, 1);
        luaL_requiref(L, LUA_TABLIBNAME, luaopen_table, 1);
        luaL_requiref(L, LUA_STRLIBNAME, luaopen_string, 1);
        luaL_requiref(L, LUA_MATHLIBNAME, luaopen_math, 1);
        lua_pop(L, 4);

        for (const auto name : { "dofile", "loadfile", "load", "loadstring", "require", "collectgarbage" })
        {
            lua_pushnil(L);
            lua_setglobal(L, name);
        }

        lua_newtable(L);
        const luaL_Reg funcs[] = { { "call", call },
                                   { "pcall", pcall },
                                   { "error_reply", errorReply },
                                   { "status_reply", statusReply },
                                   { "sha1hex", sha1hex },
                                   { "log", noop },
                                   { "setresp", noop },
                                   { "set_repl", noop },
                                   { "replicate_commands", noop },
                                   { nullptr, nullptr } };
        luaL_setfuncs(L, funcs, 0);
        int level = 0;
        for (const auto name : { "LOG_DEBUG", "LOG_VERBOSE", "LOG_NOTICE", "LOG_WARNING" })
        {
            lua_pushinteger(L, level++);
            lua_setfield(L, -2, name);
        }
        lua_pushstring(L, redisVersion);
        lua_setfield(L, -2, "REDIS_VERSION");
        lua_setglobal(L, "redis");

        lua_newtable(L);
        lua_setfield(L, LUA_REGISTRYINDEX, scriptsRegistryKey);

        lua_sethook(L, luaTimeoutHook, LUA_MASKCOUNT, 10000);

        luaHolder.L = L;
        return L;
    }

private:
    static int callImpl(lua_State* L, const bool isPcall)
    {
        auto* ctx = currentScript;
        if (ctx == nullptr)
            return luaL_error(L, "redis.call outside of script");

        const int argc = lua_gettop(L);
        if (argc == 0)
            return luaL_error(L, "Please specify at least one argument for this redis lib call");

        RedisCommand cmd;
        cmd.args.reserve(static_cast<size_t>(argc));
        for (int i = 1; i <= argc; ++i)
        {
            if (lua_type(L, i) == LUA_TSTRING)
            {
                size_t len;
                const char* str = lua_tolstring(L, i, &len);
                cmd.args.emplace_back(str, len);
            }
            else if (lua_type(L, i) == LUA_TNUMBER)
            {
                if (lua_isinteger(L, i))
                {
                    cmd.args.push_back(std::to_string(lua_tointeger(L, i)));
                }
                else
                {
                    const auto num = lua_tonumber(L, i);
                    if (std::trunc(num) == num && std::abs(num) < 9.2e18)
                        cmd.args.push_back(std::to_string(static_cast<int64_t>(num)));
                    else
                        cmd.args.push_back(formatDouble(num));
                }
            }
            else
            {
                return luaL_error(L, "Lua redis lib command arguments must be strings or integers");
            }
        }

        const auto name = toUpper(cmd.args[0]);
        static const std::unordered_set<std::string_view> notAllowed = { "MULTI", "EXEC", "DISCARD", "WATCH", "UNWATCH", "SUBSCRIBE",
                                                                         "PSUBSCRIBE", "UNSUBSCRIBE", "PUNSUBSCRIBE", "EVAL", "EVALSHA",
                                                                         "EVAL_RO", "EVALSHA_RO", "SCRIPT", "QUIT", "RESET", "HELLO",
                                                                         "AUTH", "SELECT", "CLIENT" };

        if (ctx->needsGlobalLock)
            return luaL_error(L, "ERR script has to be run again with the global lock");

        if (!inGlobalLock)
        {
            std::optional<std::string> undeclared;
            if (accessesAllKeys(name))
                undeclared = "*";
            else
            {
                for (const auto& key : commandKeys(name, cmd.args))
                {
                    if (!ctx->declaredKeys.contains(key))
                    {
                        undeclared = key;
                        break;
                    }
                }
            }

            if (undeclared)
            {
                ctx->server->markGlobalScript(ctx->sha);
                if (ctx->canRetry && !ctx->wrote)
                {
                    // Nothing modified yet. Run the script again with the global lock
                    ctx->needsGlobalLock = true;
                    return luaL_error(L, "ERR script has to be run again with the global lock");
                }
                if (!FLAGS_redis_script_allow_undeclared_keys)
                {
                    const auto msg = fmt::format("ERR script tried accessing undeclared key '{}' {}. Declare all keys (KEYS) or enable "
                                                 "redis_script_always_global or redis_script_allow_undeclared_keys",
                                                 undeclared->substr(0, 128), ctx->wrote ? "after modifying data" : "in a transaction");
                    lua_pushlstring(L, msg.data(), msg.size());
                    return lua_error(L);
                }
            }
        }

        std::string reply;
        if (notAllowed.contains(name))
        {
            reply = resp::error("ERR This Redis command is not allowed from script");
        }
        else if (!isKnownCommand(name))
        {
            reply = resp::error("ERR Unknown Redis command called from script");
        }
        else
        {
            try
            {
                auto res = ctx->server->execCommand(*ctx->state, cmd, name, *ctx->conn);
                reply = materializeReply(res);
                if (name == "PUBLISH" || (currentTx != nullptr && currentTx->needCommit))
                    ctx->wrote = true;
            }
            catch (const std::exception& e)
            {
                XLOGF(ERR, "Exception executing Redis command from script: {}", e.what());
                reply = resp::error("ERR internal error");
            }
        }

        bool isError = false;
        std::string errorMsg;
        pushRespToLua(L, reply, 0, isError, errorMsg);
        if (isError && !isPcall)
        {
            // Raise the error table {err=...}
            return lua_error(L);
        }
        return 1;
    }
};

RedisServer::ExecResult RedisServer::runScript(ConnState& state, const std::string& sha, const std::string& body, const std::vector<std::string>& keys,
                                               const std::vector<std::string>& argv, const std::shared_ptr<RedisConnection>& conn, const bool canRetry)
{
    lua_State* L = LuaScript::getState();
    if (L == nullptr)
        return { resp::error("ERR could not create Lua state") };

    const int top = lua_gettop(L);
    SCOPE_EXIT { lua_settop(L, top); };

    // Compiled function (cached per thread)
    lua_getfield(L, LUA_REGISTRYINDEX, scriptsRegistryKey);
    lua_getfield(L, -1, sha.c_str());
    if (!lua_isfunction(L, -1))
    {
        lua_pop(L, 1);
        if (luaL_loadbuffer(L, body.data(), body.size(), "@user_script") != LUA_OK)
        {
            std::string err = lua_tostring(L, -1) ? lua_tostring(L, -1) : "unknown error";
            return { resp::error(fmt::format("ERR Error compiling script (new function): {}", err)) };
        }
        lua_pushvalue(L, -1);
        lua_setfield(L, -3, sha.c_str());
    }

    // Fresh environment per call (globals of scripts don't leak into other calls), falling back to the globals
    lua_newtable(L);
    const auto pushArray = [L](const std::vector<std::string>& vals)
    {
        lua_createtable(L, static_cast<int>(vals.size()), 0);
        for (size_t i = 0; i < vals.size(); ++i)
        {
            lua_pushlstring(L, vals[i].data(), vals[i].size());
            lua_rawseti(L, -2, static_cast<lua_Integer>(i + 1));
        }
    };
    pushArray(keys);
    lua_setfield(L, -2, "KEYS");
    pushArray(argv);
    lua_setfield(L, -2, "ARGV");
    lua_newtable(L);
    lua_pushglobaltable(L);
    lua_setfield(L, -2, "__index");
    lua_setmetatable(L, -2);
    // _ENV is the first upvalue of the main chunk
    lua_setupvalue(L, -2, 1);

    // Commands of the script use RESP2 (converted to Lua values)
    ConnState scriptState;
    scriptState.id = state.id;
    scriptState.accessKey = state.accessKey;
    scriptState.name = state.name;

    ScriptCtx ctx{ .server = this,
                   .state = &scriptState,
                   .conn = &conn,
                   .deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(FLAGS_redis_script_timeout_ms),
                   .sha = sha,
                   .declaredKeys = std::unordered_set<std::string>(keys.begin(), keys.end()),
                   .canRetry = canRetry };
    currentScript = &ctx;
    SCOPE_EXIT { currentScript = nullptr; };

    const auto rc = lua_pcall(L, 0, 1, 0);
    // Also if the script caught the error
    if (ctx.needsGlobalLock)
        return { .needsGlobalLock = true };

    if (rc != LUA_OK)
    {
        std::string err;
        if (lua_istable(L, -1))
        {
            lua_getfield(L, -1, "err");
            if (lua_type(L, -1) == LUA_TSTRING)
                err = lua_tostring(L, -1);
            lua_pop(L, 1);
        }
        else if (lua_type(L, -1) == LUA_TSTRING)
        {
            err = lua_tostring(L, -1);
        }
        if (err.empty())
            err = "ERR unknown script error";
        else if (err.find(' ') == std::string::npos || !std::isupper(static_cast<unsigned char>(err[0])) || err.starts_with("user_script:"))
            err = fmt::format("ERR {} script: {}", err, sha);
        std::replace(err.begin(), err.end(), '\r', ' ');
        std::replace(err.begin(), err.end(), '\n', ' ');
        return { resp::error(err) };
    }

    std::string reply;
    luaToResp(L, reply, state);
    return { reply };
}

RedisServer::ExecResult RedisServer::cmdEval(ConnState& state, const std::vector<std::string>& args, const std::string& cmd,
                                             const std::shared_ptr<RedisConnection>& conn)
{
    if (args.size() < 3)
        return { resp::wrongArgs(toLower(cmd)) };

    int64_t numKeys;
    if (!parseInt64(args[2], numKeys))
        return { resp::notInteger };
    if (numKeys < 0)
        return { resp::error("ERR Number of keys can't be negative") };
    if (static_cast<size_t>(numKeys) > args.size() - 3)
        return { resp::error("ERR Number of keys can't be greater than number of args") };

    std::string sha, body;
    if (cmd == "EVAL" || cmd == "EVAL_RO")
    {
        body = args[1];
        sha = sha1Hex(body);
        std::scoped_lock lock(scriptsMutex);
        scripts.try_emplace(sha, body);
    }
    else
    {
        sha = toLower(args[1]);
        std::scoped_lock lock(scriptsMutex);
        auto it = scripts.find(sha);
        if (it == scripts.end())
            return { resp::error("NOSCRIPT No matching script. Please use EVAL.") };
        body = it->second;
    }

    const std::vector<std::string> keys(args.begin() + 3, args.begin() + 3 + numKeys);
    const std::vector<std::string> argv(args.begin() + 3 + numKeys, args.end());
    for (const auto& key : keys)
    {
        if (auto err = checkKey(key))
            return { *err };
    }

    // Already in a transaction (EXEC), which holds the key locks. Cannot be run again
    if (currentTx != nullptr)
        return runScript(state, sha, body, keys, argv, conn, false);

    TxContext tx;
    tx.inScript = true;
    ExecResult res;
    {
        // Atomic with respect to the declared keys
        MultiKeyLock lock(*this, keys);
        lock.touch();
        currentTx = &tx;
        SCOPE_EXIT { currentTx = nullptr; };
        res = runScript(state, sha, body, keys, argv, conn, !inGlobalLock);

        // Commit modified entries of large collections before releasing the locks (see finishCollectionWrite)
        if (tx.needCommit && tx.largeWrites && !sfs.listings_include_uncommitted())
        {
            if (!S3Handler::commitStorage(sfs))
                return { resp::error("ERR storage commit failed") };
            tx.needCommit = false;
        }
    }

    if (tx.needCommit && !commit())
        return { resp::error("ERR storage commit failed") };
    return res;
}

RedisServer::ExecResult RedisServer::cmdScript(ConnState& /*state*/, const std::vector<std::string>& args)
{
    if (args.size() < 2)
        return { resp::wrongArgs("script") };

    const auto sub = toUpper(args[1]);
    if (sub == "LOAD" && args.size() == 3)
    {
        const auto sha = sha1Hex(args[2]);
        // Check that it compiles
        lua_State* L = LuaScript::getState();
        if (L == nullptr)
            return { resp::error("ERR could not create Lua state") };
        const int top = lua_gettop(L);
        SCOPE_EXIT { lua_settop(L, top); };
        if (luaL_loadbuffer(L, args[2].data(), args[2].size(), "@user_script") != LUA_OK)
        {
            std::string err = lua_tostring(L, -1) ? lua_tostring(L, -1) : "unknown error";
            return { resp::error(fmt::format("ERR Error compiling script (new function): {}", err)) };
        }

        std::scoped_lock lock(scriptsMutex);
        scripts.try_emplace(sha, args[2]);
        return { resp::bulk(sha) };
    }
    if (sub == "EXISTS" && args.size() >= 3)
    {
        std::string reply = resp::arrayHeader(args.size() - 2);
        std::scoped_lock lock(scriptsMutex);
        for (size_t i = 2; i < args.size(); ++i)
            reply += resp::integer(scripts.contains(toLower(args[i])) ? 1 : 0);
        return { reply };
    }
    if (sub == "FLUSH")
    {
        std::scoped_lock lock(scriptsMutex);
        scripts.clear();
        return { resp::ok };
    }
    if (sub == "KILL")
        return { resp::error("NOTBUSY No scripts in execution right now.") };

    return { resp::error(fmt::format("ERR unknown subcommand or wrong number of arguments for '{}'", args[1])) };
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
