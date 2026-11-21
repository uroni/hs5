/**
 * Copyright Martin Raiber. All Rights Reserved.
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */
#pragma once

// Internals of KeyStore and the protocol front-ends using it

#include "Buckets.h"
#include "ContentType.h"
#include "KeyStore.h"
#include "ObjMetadata.h"
#include "ObjectReader.h"
#include "PayloadHash.h"
#include "SingleFileStorage.h"
#include "data.h"
#include "s3handler.h"
#include <algorithm>
#include <cerrno>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstring>
#include <folly/ScopeGuard.h>
#include <folly/lang/Bits.h>
#include <folly/logging/xlog.h>
#include <gflags/gflags.h>
#include <map>
#include <openssl/evp.h>
#include <openssl/md5.h>
#include <set>
#include <unordered_map>
#include <unordered_set>

DECLARE_bool(disable_md5_etag);
DECLARE_int64(redis_max_value_size);
DECLARE_int64(redis_collection_inline_max_entries);
DECLARE_int64(redis_collection_inline_max_bytes);

namespace kvdetail
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
        inline const std::string ok = "+OK\r\n";

        // RESP3 has a single null type
        template <typename State>
        inline const std::string& nullBulk(const State& state)
        {
            static const std::string resp2 = "$-1\r\n";
            static const std::string resp3 = "_\r\n";
            return state.resp3 ? resp3 : resp2;
        }

        template <typename State>
        inline const std::string& nullArray(const State& state)
        {
            static const std::string resp2 = "*-1\r\n";
            static const std::string resp3 = "_\r\n";
            return state.resp3 ? resp3 : resp2;
        }
        inline const std::string emptyArray = "*0\r\n";

        inline std::string simple(const std::string_view str)
        {
            return fmt::format("+{}\r\n", str);
        }

        inline std::string error(const std::string_view str)
        {
            return fmt::format("-{}\r\n", str);
        }

        inline std::string integer(const int64_t i)
        {
            return fmt::format(":{}\r\n", i);
        }

        inline std::string arrayHeader(const size_t n)
        {
            return fmt::format("*{}\r\n", n);
        }

        inline void appendBulk(std::string& out, const std::string_view str)
        {
            out += fmt::format("${}\r\n", str.size());
            out.append(str);
            out += "\r\n";
        }

        inline std::string bulk(const std::string_view str)
        {
            std::string ret;
            appendBulk(ret, str);
            return ret;
        }

        inline std::string wrongArgs(const std::string_view cmd)
        {
            return error(fmt::format("ERR wrong number of arguments for '{}' command", cmd));
        }

        inline const std::string notInteger = error("ERR value is not an integer or out of range");
        inline const std::string syntaxError = error("ERR syntax error");
        inline const std::string wrongType = error("WRONGTYPE Operation against a key holding the wrong kind of value");
    } // namespace resp

    inline std::string toUpper(const std::string_view str)
    {
        std::string ret(str);
        for (auto& ch : ret)
            ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
        return ret;
    }

    inline std::string toLower(const std::string_view str)
    {
        std::string ret(str);
        for (auto& ch : ret)
            ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        return ret;
    }

    // Parses an integer like Redis (no leading zeros, no '+', no whitespace)
    inline bool parseInt64(const std::string_view str, int64_t& val)
    {
        if (str.empty() || str.size() > 20)
            return false;

        const auto res = std::from_chars(str.data(), str.data() + str.size(), val);
        if (res.ec != std::errc() || res.ptr != str.data() + str.size())
            return false;

        return std::to_string(val) == str;
    }

    inline bool parseTimeout(const std::string& str, double& timeout)
    {
        if (str.empty())
            return false;

        char* end = nullptr;
        timeout = std::strtod(str.c_str(), &end);
        return end == str.c_str() + str.size() && std::isfinite(timeout);
    }

    // Glob-style pattern matching like Redis (*, ?, [abc], [^a-z], \x)
    inline bool globMatch(std::string_view pat, std::string_view str, const int nesting = 0)
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
    inline std::string globPrefix(const std::string_view pat)
    {
        const auto pos = pat.find_first_of("*?[\\");
        return std::string(pat.substr(0, pos));
    }

    inline bool isTombstone(const std::string_view md5sum)
    {
        return md5sum.size() == 1 && md5sum[0] == metadata_tombstone;
    }

    // Redis types are stored as content type. List elements have this content type, all other objects are strings
    inline const ContentType& listContentType()
    {
        static const ContentType contentType = contentTypeFromStr("application/cdmi-queue");
        assert(contentType.contentType != 0);
        return contentType;
    }

    // Content type of strings written via the Redis protocol
    inline const ContentType& stringContentType()
    {
        static const ContentType contentType = contentTypeFromStr("text/plain");
        assert(contentType.contentType != 0);
        return contentType;
    }

    inline bool isListElement(const std::string& md5sum)
    {
        return !isTombstone(md5sum) && S3Handler::contentTypeIdFromMetadata(md5sum) == listContentType().contentType;
    }

    // Writes data at offset of an object to its extents
    inline int writeToExtents(SingleFileStorage& sfs, const std::vector<SingleFileStorage::Ext>& extents, const size_t offset,
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
    inline size_t findHeaderEnd(const char* data, const size_t len, const size_t pos, std::string& err)
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
    inline ParseResult parseInline(const char* data, const size_t len, size_t& consumed, std::vector<std::string>& args, std::string& err)
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
    inline bool isStreamableArg(const std::string_view cmdName, const size_t idx)
    {
        if (cmdName == "SET" || cmdName == "SETNX")
            return idx == 2;
        if (cmdName == "MSET")
            return idx >= 2 && idx % 2 == 0;
        if (cmdName == "LPUSH" || cmdName == "RPUSH" || cmdName == "LPUSHX" || cmdName == "RPUSHX")
            return idx >= 2;
        return false;
    }

    using KeyType = KeyStore::KeyType;

    // Content types of hashes, sets and sorted sets (stored as one serialized object). Registered content types (with
    // id), like the list content type
    constexpr std::string_view hashContentTypeStr = "application/cdmi-object";
    constexpr std::string_view setContentTypeStr = "application/cdmi-container";
    constexpr std::string_view zsetContentTypeStr = "application/cdmi-domain";

    inline const ContentType& collectionContentType(const KeyType type)
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

    inline KeyType keyTypeFromContentType(const ContentType& contentType)
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
    std::pair<KeyType, std::optional<KeyStore::ExpiryInfo>> keyInfoFromMetadata(const std::string& md5sum);

    inline KeyType readerKeyType(ObjectReader& reader)
    {
        const auto objMetadata = reader.GetObjMetadata();
        if (objMetadata == nullptr)
            return KeyType::String;
        return keyTypeFromContentType(objMetadata->contentType);
    }

    inline const char* keyTypeName(const KeyType type)
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

    inline int64_t nowMs()
    {
        return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    }

    // Key expiry is stored in the object metadata (ObjMetadata::LockInfo) of the value. Lists store it after the count in
    // their counter (varint expiry time, varint index version), which is deleted with the last element.
    // Additionally there is an index ordered by expiry time in the counters bucket (see buckets::getCountersBucket),
    // which also has the list counters (Redis key, version 0): Key 0xff 'i' + big endian expiry time, with a unique
    // version > 0 and the Redis key as md5sum. Byte 0xff is reserved (not allowed as first byte of Redis keys), and
    // the version is never 0, so they cannot overlap with counters
    inline const std::string reservedKeyPrefix("\xff", 1);
    inline const std::string expiryIndexPrefix("\xffi", 2);

    inline std::optional<KeyStore::ExpiryInfo> expiryFromObjMetadata(const ObjMetadata* objMetadata)
    {
        if (objMetadata == nullptr || !objMetadata->lockInfo || !objMetadata->lockInfo->redisExpiry())
            return std::nullopt;
        return KeyStore::ExpiryInfo{ .ms = objMetadata->lockInfo->redisExpiryMs, .indexVersion = objMetadata->lockInfo->redisExpiryIndexVersion };
    }

    inline std::optional<KeyStore::ExpiryInfo> expiryFromMetadata(const std::string& md5sum)
    {
        if (isTombstone(md5sum))
            return std::nullopt;
        const auto objMetadata = S3Handler::objMetadataFromMetadata(md5sum);
        return expiryFromObjMetadata(objMetadata.get());
    }

    inline bool isExpired(const std::optional<KeyStore::ExpiryInfo>& expiry)
    {
        return expiry && expiry->ms <= nowMs();
    }

    inline std::pair<KeyType, std::optional<KeyStore::ExpiryInfo>> keyInfoFromMetadata(const std::string& md5sum)
    {
        if (isTombstone(md5sum))
            return { KeyType::None, std::nullopt };
        const auto objMetadata = S3Handler::objMetadataFromMetadata(md5sum);
        if (!objMetadata)
            return { KeyType::String, std::nullopt };
        return { keyTypeFromContentType(objMetadata->contentType), expiryFromObjMetadata(objMetadata.get()) };
    }

    inline std::string expiryIndexKey(const int64_t expiryMs)
    {
        const auto val = folly::Endian::big(static_cast<uint64_t>(expiryMs));
        std::string ret = expiryIndexPrefix;
        ret.append(reinterpret_cast<const char*>(&val), sizeof(val));
        return ret;
    }

    inline std::optional<int64_t> parseExpiryIndexKey(const std::string_view key)
    {
        uint64_t val;
        if (key.size() != expiryIndexPrefix.size() + sizeof(val) || !key.starts_with(expiryIndexPrefix))
            return std::nullopt;
        std::memcpy(&val, key.data() + expiryIndexPrefix.size(), sizeof(val));
        return static_cast<int64_t>(folly::Endian::big(val));
    }

    // Parses a double like Redis (also inf, -inf, +inf). Rejects NaN
    inline bool parseDouble(const std::string_view str, double& val)
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

    inline std::string formatDouble(const double val)
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

    inline bool parseScoreBound(const std::string_view str, ScoreBound& bound)
    {
        bound.exclusive = !str.empty() && str[0] == '(';
        return parseDouble(bound.exclusive ? str.substr(1) : str, bound.val);
    }

    inline bool aboveMin(const double score, const ScoreBound& min)
    {
        return min.exclusive ? score > min.val : score >= min.val;
    }

    inline bool belowMax(const double score, const ScoreBound& max)
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

    inline thread_local TxContext* currentTx = nullptr;

    // This thread holds the global lock exclusively (no other command runs concurrently)
    inline thread_local bool inGlobalLock = false;

    // Positions of keys in the arguments of a command
    struct KeySpec
    {
        // Index of the first key. 0 if the command has no keys
        int first = 0;
        // Index of the last key. Negative: relative to the end (-1 is the last argument)
        int last = 0;
        int step = 1;
    };

    inline const std::unordered_map<std::string_view, KeySpec>& keySpecs()
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
    inline const std::unordered_set<std::string_view>& keylessCommands()
    {
        static const std::unordered_set<std::string_view> cmds = { "PING", "ECHO", "QUIT", "AUTH", "HELLO", "SELECT", "CLIENT", "COMMAND",
                                                                   "INFO", "RESET", "SCAN", "KEYS", "DBSIZE", "FLUSHDB", "FLUSHALL",
                                                                   "PUBLISH", "SUBSCRIBE", "UNSUBSCRIBE", "PSUBSCRIBE", "PUNSUBSCRIBE",
                                                                   "MULTI", "EXEC", "DISCARD", "UNWATCH", "SCRIPT", "TIME", "EVAL",
                                                                   "EVALSHA", "EVAL_RO", "EVALSHA_RO" };
        return cmds;
    }

    inline bool isKnownCommand(const std::string& cmd)
    {
        return keySpecs().contains(cmd) || keylessCommands().contains(cmd);
    }

    // Keys accessed by a command (upper case name cmd). EVAL/EVALSHA: the declared keys
    inline std::vector<std::string> commandKeys(const std::string& cmd, const std::vector<std::string>& args)
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

    inline std::string sha1Hex(const std::string_view data)
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

    // Fields/members longer than this are stored with their SHA-256 in the key of large collections (LMDB key size limit)
    constexpr size_t maxFieldKeyLen = 256;

    inline char collectionTag(const KeyType type)
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

    inline std::string encodeKey(const std::string_view key)
    {
        CWData data;
        data.addVarInt(static_cast<int64_t>(key.size()));
        data.addBuffer(key.data(), key.size());
        return std::string(data.getDataPtr(), data.getDataSize());
    }

    // Prefix of the entries of a large collection (in the counters bucket)
    inline std::string collectionPrefix(const KeyType type, const std::string_view key)
    {
        std::string ret = reservedKeyPrefix;
        ret.push_back(collectionTag(type));
        ret += encodeKey(key);
        return ret;
    }

    // Key of the counter of the entries of a large collection (maintained by the storage engine)
    inline std::string collectionCounterKey(const std::string_view key)
    {
        std::string ret = reservedKeyPrefix;
        ret.push_back('c');
        ret += encodeKey(key);
        return ret;
    }

    inline bool isLongField(const std::string_view field)
    {
        return field.size() > maxFieldKeyLen;
    }

    inline std::string sha256(const std::string_view data)
    {
        unsigned char md[EVP_MAX_MD_SIZE];
        unsigned int mdLen = 0;
        if (EVP_Digest(data.data(), data.size(), md, &mdLen, EVP_sha256(), nullptr) != 1)
            return std::string();
        return std::string(reinterpret_cast<const char*>(md), mdLen);
    }

    // Field in an entry key: 0 + field, or 1 + SHA-256 of long fields (the field is then stored in the entry)
    inline std::string encodeField(const std::string_view field)
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
    inline std::string sortableScore(double score)
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

    inline double scoreFromSortable(const std::string_view data)
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
    inline std::string collectionCounterKeyOfEntry(const std::string_view entryKey)
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
} // namespace kvdetail

using namespace kvdetail;

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
class KeyStore::PreparedValue
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

// Exclusively locks the key lock of a key (for modifying it). Does nothing in a transaction/script (which holds the
// locks of its keys)
class KeyStore::KeyLock
{
public:
    KeyLock(KeyStore& server, const std::string& key) NO_THREAD_SAFETY_ANALYSIS
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
class KeyStore::MultiKeyLock
{
public:
    MultiKeyLock(KeyStore& server, const std::vector<std::string>& args, const size_t start, const size_t step, const bool exclusive)
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
    MultiKeyLock(KeyStore& server, const std::vector<std::string>& keys)
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
    void lockIdxs(KeyStore& srv, std::vector<size_t> idxs, const bool touchEpochs) NO_THREAD_SAFETY_ANALYSIS
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
    KeyStore* server = nullptr;
    std::vector<size_t> locks;
};

// Small hash, set or sorted set, serialized inline in the object metadata (ObjMetadata::redisData) of the value: varint
// format version (1), varint number of elements, then the elements (hash: field, value; set: member; sorted set:
// member, score as little endian double). Strings are varint length + data. Larger collections are stored as one entry
// per element (see LargeCollOps)
struct KeyStore::Collection
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
class KeyStore::CollOps
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

class KeyStore::SmallCollOps : public CollOps
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
class KeyStore::LargeCollOps : public CollOps
{
public:
    LargeCollOps(KeyStore& server, const int64_t bucketId, const std::string& key, const KeyType type)
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

    KeyStore& server;
    SingleFileStorage& sfs;
    const int64_t countersBucket;
    const std::string key;
    const KeyType type;
    const std::string prefix;
};

// Shared lock of a key (field-level modifications of large collections). Does nothing in a transaction/script
class KeyStore::SharedKeyLock
{
public:
    SharedKeyLock(KeyStore& server, const std::string& key) NO_THREAD_SAFETY_ANALYSIS
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
class KeyStore::FieldLocks
{
public:
    FieldLocks(KeyStore& server, const std::string& key, const std::vector<std::string>& fields) NO_THREAD_SAFETY_ANALYSIS
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
    KeyStore& server;
    std::vector<size_t> idxs;
};

struct KeyStore::CollState
{
    bool exists = false;
    // Stored with one entry per element (otherwise inline, in small)
    bool large = false;
    Collection small;
    std::optional<ExpiryInfo> expiry;
};
// Group lock of a key (see collGroupLocks). Does nothing in a transaction/script
class KeyStore::CollGroupLock
{
public:
    static constexpr int writers = 0;
    static constexpr int readers = 1;

    CollGroupLock(KeyStore& server, const std::string& key, const int group)
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
struct KeyStore::CollLocks
{
    std::optional<KeyLock> exclusive;
    std::optional<SharedKeyLock> shared;
    std::optional<CollGroupLock> group;
    std::optional<FieldLocks> fields;
};
