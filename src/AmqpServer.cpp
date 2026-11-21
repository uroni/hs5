/**
 * Copyright Martin Raiber. All Rights Reserved.
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */
#include "AmqpServer.h"
#include "Auth.h"
#include "KeyStoreDetail.h"
#include "Policy.h"
#include <folly/Random.h>
#include <folly/String.h>
#include <folly/executors/GlobalExecutor.h>
#include <folly/executors/SerialExecutor.h>
#include <folly/executors/thread_factory/NamedThreadFactory.h>
#include <folly/io/IOBuf.h>
#include <folly/io/async/AsyncSocket.h>
#include <folly/io/async/AsyncTimeout.h>
#include <sodium.h>

DEFINE_int64(amqp_max_message_size, 128 * 1024 * 1024, "Max size of AMQP message bodies");
DEFINE_int32(amqp_frame_max, 131072, "Max AMQP frame size offered to clients");
DEFINE_int32(amqp_heartbeat, 60, "AMQP heartbeat interval offered to clients (seconds, 0 to disable)");
DEFINE_int64(amqp_memory_message_max, 64 * 1024, "Max body size of AMQP messages kept in memory (delivered without reading them from storage, 0 to disable)");
DEFINE_int64(amqp_memory_max_mb, 256, "Max memory used by AMQP messages kept in memory (MiB)");

namespace
{
    constexpr uint8_t frameMethod = 1;
    constexpr uint8_t frameHeader = 2;
    constexpr uint8_t frameBody = 3;
    constexpr uint8_t frameHeartbeat = 8;
    constexpr uint8_t frameEnd = 0xCE;
    // Frame size before connection.tune-ok
    constexpr uint32_t frameMinSize = 4096;
    // Stop reading from a connection if this many bytes of frames are not processed yet
    constexpr size_t maxPendingInput = 64 * 1024 * 1024;
    // Don't deliver more messages to a connection while this much data is waiting to be sent
    constexpr size_t maxPendingOutput = 16 * 1024 * 1024;
    constexpr uint16_t maxChannels = 2047;
    // Message bodies at least this large are written to the data file while they are received
    constexpr uint64_t inStreamThreshold = 64 * 1024;
    // Message bodies larger than this are read from the data file while they are sent, in chunks of this size
    constexpr int64_t outStreamChunk = 1024 * 1024;
    // Read the next chunk of a streamed message only if less than this is waiting to be sent
    constexpr size_t outStreamHigh = 4 * 1024 * 1024;

    // Class and method ids
    constexpr uint16_t classConnection = 10, classChannel = 20, classExchange = 40, classQueue = 50, classBasic = 60, classConfirm = 85,
                       classTx = 90;

    // Method ids
    constexpr uint16_t connectionStart = 10, connectionStartOk = 11, connectionTune = 30, connectionTuneOk = 31, connectionOpen = 40, connectionOpenOk = 41, connectionClose = 50, connectionCloseOk = 51;
    constexpr uint16_t channelOpen = 10, channelOpenOk = 11, channelFlow = 20, channelFlowOk = 21, channelClose = 40, channelCloseOk = 41;
    constexpr uint16_t exchangeDeclare = 10, exchangeDeclareOk = 11, exchangeDelete = 20, exchangeDeleteOk = 21;
    constexpr uint16_t queueDeclare = 10, queueDeclareOk = 11, queueBind = 20, queueBindOk = 21, queuePurge = 30, queuePurgeOk = 31, queueDelete = 40, queueDeleteOk = 41, queueUnbind = 50, queueUnbindOk = 51;
    constexpr uint16_t basicQos = 10, basicQosOk = 11, basicConsume = 20, basicConsumeOk = 21, basicCancel = 30, basicCancelOk = 31, basicPublish = 40, basicReturn = 50, basicDeliver = 60, basicGet = 70, basicGetOk = 71, basicGetEmpty = 72, basicAck = 80, basicReject = 90, basicRecoverAsync = 100, basicRecover = 110, basicRecoverOk = 111, basicNack = 120;
    constexpr uint16_t confirmSelect = 10, confirmSelectOk = 11;
    constexpr uint16_t exchangeBind = 30, exchangeBindOk = 31, exchangeUnbind = 40, exchangeUnbindOk = 51;
    constexpr uint16_t txSelect = 10, txSelectOk = 11, txCommit = 20, txCommitOk = 21, txRollback = 30, txRollbackOk = 31;

    // Reply codes
    constexpr uint16_t replySuccess = 200, contentTooLarge = 311, noRoute = 312, accessRefused = 403, notFound = 404, resourceLocked = 405,
                       preconditionFailed = 406, frameError = 501, syntaxError = 502, commandInvalid = 503, channelError = 504,
                       unexpectedFrame = 505, resourceError = 506, notAllowed = 530, notImplemented = 540, internalError = 541;

    struct FieldValue;
    using FieldTable = std::vector<std::pair<std::string, FieldValue>>;

    struct FieldValue
    {
        char type = 'V';
        bool b = false;
        int64_t i = 0;
        double d = 0;
        std::string s;
        std::shared_ptr<FieldTable> table;
        std::vector<FieldValue> array;

        std::optional<int64_t> asInt() const
        {
            switch (type)
            {
                case 'b':
                case 'B':
                case 's':
                case 'u':
                case 'I':
                case 'i':
                case 'l':
                case 'L':
                case 'T':
                    return i;
                case 'f':
                case 'd':
                    return static_cast<int64_t>(d);
                case 'S':
                {
                    int64_t val;
                    if (parseInt64(s, val))
                        return val;
                    return std::nullopt;
                }
                default:
                    return std::nullopt;
            }
        }
    };

    const FieldValue* tableGet(const FieldTable& table, const std::string_view name)
    {
        for (const auto& [key, value] : table)
        {
            if (key == name)
                return &value;
        }
        return nullptr;
    }

    // Reads AMQP data types (big endian). Sets ok to false on error
    class Reader
    {
    public:
        Reader(const char* data, size_t len)
            : data(reinterpret_cast<const uint8_t*>(data)), len(len)
        {
        }

        explicit Reader(const std::string_view str)
            : Reader(str.data(), str.size())
        {
        }

        bool ok = true;

        bool has(const size_t n)
        {
            if (pos + n > len)
                ok = false;
            return ok;
        }

        uint8_t octet()
        {
            if (!has(1))
                return 0;
            return data[pos++];
        }

        uint16_t short_()
        {
            return readBig<uint16_t>();
        }

        uint32_t long_()
        {
            return readBig<uint32_t>();
        }

        uint64_t longlong()
        {
            return readBig<uint64_t>();
        }

        std::string bytes(const size_t n)
        {
            if (!has(n))
                return std::string();
            std::string ret(reinterpret_cast<const char*>(data + pos), n);
            pos += n;
            return ret;
        }

        std::string shortstr()
        {
            return bytes(octet());
        }

        std::string longstr()
        {
            return bytes(long_());
        }

        // Bits are packed into octets (only consecutive bits)
        bool bit()
        {
            if (bitPos == 0 || bitPos == 8)
            {
                bitByte = octet();
                bitPos = 0;
            }
            return (bitByte >> bitPos++) & 1;
        }

        void endBits()
        {
            bitPos = 0;
        }

        FieldValue value(const int depth = 0)
        {
            FieldValue ret;
            ret.type = static_cast<char>(octet());
            switch (ret.type)
            {
                case 't':
                    ret.b = octet() != 0;
                    ret.i = ret.b ? 1 : 0;
                    break;
                case 'b':
                    ret.i = static_cast<int8_t>(octet());
                    break;
                case 'B':
                    ret.i = octet();
                    break;
                case 's':
                    ret.i = static_cast<int16_t>(short_());
                    break;
                case 'u':
                    ret.i = short_();
                    break;
                case 'I':
                    ret.i = static_cast<int32_t>(long_());
                    break;
                case 'i':
                    ret.i = long_();
                    break;
                case 'l':
                    ret.i = static_cast<int64_t>(longlong());
                    break;
                case 'L':
                case 'T':
                    ret.i = static_cast<int64_t>(longlong());
                    break;
                case 'f':
                {
                    const uint32_t bits = long_();
                    float f;
                    std::memcpy(&f, &bits, sizeof(f));
                    ret.d = f;
                    break;
                }
                case 'd':
                {
                    const uint64_t bits = longlong();
                    std::memcpy(&ret.d, &bits, sizeof(ret.d));
                    break;
                }
                case 'D':
                    ret.s = bytes(5);
                    break;
                case 'S':
                case 'x':
                    ret.s = longstr();
                    break;
                case 'F':
                    ret.table = std::make_shared<FieldTable>(table(depth + 1));
                    break;
                case 'A':
                {
                    const auto size = long_();
                    if (!has(size) || depth > 32)
                    {
                        ok = false;
                        break;
                    }
                    Reader sub(reinterpret_cast<const char*>(data + pos), size);
                    while (sub.ok && sub.pos < sub.len)
                        ret.array.push_back(sub.value(depth + 1));
                    ok = ok && sub.ok;
                    pos += size;
                    break;
                }
                case 'V':
                    break;
                default:
                    ok = false;
                    break;
            }
            return ret;
        }

        FieldTable table(const int depth = 0)
        {
            FieldTable ret;
            const auto size = long_();
            if (!has(size) || depth > 32)
            {
                ok = false;
                return ret;
            }
            Reader sub(reinterpret_cast<const char*>(data + pos), size);
            while (sub.ok && sub.pos < sub.len)
            {
                auto key = sub.shortstr();
                auto val = sub.value(depth);
                ret.emplace_back(std::move(key), std::move(val));
            }
            ok = ok && sub.ok;
            pos += size;
            return ret;
        }

        // Raw bytes of a table (for passing it on unchanged)
        std::string rawTable()
        {
            const auto start = pos;
            table();
            return std::string(reinterpret_cast<const char*>(data + start), pos - start);
        }

        size_t remaining() const
        {
            return len - pos;
        }

        size_t position() const
        {
            return pos;
        }

    private:
        template <typename T>
        T readBig()
        {
            if (!has(sizeof(T)))
                return 0;
            T val;
            std::memcpy(&val, data + pos, sizeof(T));
            pos += sizeof(T);
            return folly::Endian::big(val);
        }

        const uint8_t* data;
        size_t len;
        size_t pos = 0;
        uint8_t bitByte = 0;
        int bitPos = 0;
    };

    class Writer
    {
    public:
        std::string out;

        Writer& octet(const uint8_t v)
        {
            flushBits();
            out.push_back(static_cast<char>(v));
            return *this;
        }

        Writer& short_(const uint16_t v)
        {
            return writeBig(v);
        }

        Writer& long_(const uint32_t v)
        {
            return writeBig(v);
        }

        Writer& longlong(const uint64_t v)
        {
            return writeBig(v);
        }

        template <typename T>
        Writer& writeBig(const T v)
        {
            flushBits();
            const T big = folly::Endian::big(v);
            out.append(reinterpret_cast<const char*>(&big), sizeof(T));
            return *this;
        }

        Writer& shortstr(const std::string_view s)
        {
            flushBits();
            const auto len = std::min(s.size(), static_cast<size_t>(255));
            out.push_back(static_cast<char>(len));
            out.append(s.substr(0, len));
            return *this;
        }

        Writer& longstr(const std::string_view s)
        {
            long_(static_cast<uint32_t>(s.size()));
            out.append(s);
            return *this;
        }

        Writer& raw(const std::string_view s)
        {
            flushBits();
            out.append(s);
            return *this;
        }

        Writer& bit(const bool b)
        {
            if (bitCount == 8)
                flushBits();
            if (bitCount == 0)
                out.push_back(0);
            if (b)
                out.back() = static_cast<char>(out.back() | (1 << bitCount));
            ++bitCount;
            return *this;
        }

        Writer& table(const FieldTable& table)
        {
            flushBits();
            Writer sub;
            for (const auto& [key, val] : table)
            {
                sub.shortstr(key);
                sub.value(val);
            }
            return longstr(sub.out);
        }

        Writer& value(const FieldValue& val)
        {
            octet(static_cast<uint8_t>(val.type));
            switch (val.type)
            {
                case 't':
                    octet(val.b ? 1 : 0);
                    break;
                case 'b':
                case 'B':
                    octet(static_cast<uint8_t>(val.i));
                    break;
                case 's':
                case 'u':
                    short_(static_cast<uint16_t>(val.i));
                    break;
                case 'I':
                case 'i':
                    long_(static_cast<uint32_t>(val.i));
                    break;
                case 'l':
                case 'L':
                case 'T':
                    longlong(static_cast<uint64_t>(val.i));
                    break;
                case 'f':
                {
                    const float f = static_cast<float>(val.d);
                    uint32_t bits;
                    std::memcpy(&bits, &f, sizeof(bits));
                    long_(bits);
                    break;
                }
                case 'd':
                {
                    uint64_t bits;
                    std::memcpy(&bits, &val.d, sizeof(bits));
                    longlong(bits);
                    break;
                }
                case 'D':
                    raw(val.s);
                    break;
                case 'S':
                case 'x':
                    longstr(val.s);
                    break;
                case 'F':
                    table(val.table ? *val.table : FieldTable());
                    break;
                case 'A':
                {
                    Writer sub;
                    for (const auto& elem : val.array)
                        sub.value(elem);
                    longstr(sub.out);
                    break;
                }
                default:
                    break;
            }
            return *this;
        }

        void flushBits()
        {
            bitCount = 0;
        }

    private:
        int bitCount = 0;
    };

    FieldValue boolValue(const bool b)
    {
        FieldValue ret;
        ret.type = 't';
        ret.b = b;
        return ret;
    }

    FieldValue stringValue(std::string s)
    {
        FieldValue ret;
        ret.type = 'S';
        ret.s = std::move(s);
        return ret;
    }

    FieldValue tableValue(FieldTable table)
    {
        FieldValue ret;
        ret.type = 'F';
        ret.table = std::make_shared<FieldTable>(std::move(table));
        return ret;
    }

    std::string frame(const uint8_t type, const uint16_t channel, const std::string_view payload)
    {
        Writer w;
        w.octet(type).short_(channel).long_(static_cast<uint32_t>(payload.size()));
        w.out.append(payload);
        w.out.push_back(static_cast<char>(frameEnd));
        return std::move(w.out);
    }

    // Method frame. args: Arguments after class and method id
    std::string methodFrame(const uint16_t channel, const uint16_t classId, const uint16_t methodId, const std::string_view args = {})
    {
        Writer w;
        w.short_(classId).short_(methodId);
        w.out.append(args);
        return frame(frameMethod, channel, w.out);
    }

    // Content header frame. props: Property flags and properties
    std::string contentHeaderFrame(const uint16_t channel, const std::string_view props, const uint64_t bodySize)
    {
        Writer w;
        w.short_(classBasic).short_(0).longlong(bodySize);
        w.out.append(props);
        return frame(frameHeader, channel, w.out);
    }

    // Body frames of (a part of) a message body
    std::string bodyFrames(const uint16_t channel, const std::string_view body, const uint32_t frameMax)
    {
        std::string ret;
        // Frame overhead is 8 bytes
        const size_t maxPayload = frameMax > 8 ? frameMax - 8 : body.size();
        for (size_t pos = 0; pos < body.size(); pos += maxPayload)
            ret += frame(frameBody, channel, body.substr(pos, std::min(maxPayload, body.size() - pos)));
        return ret;
    }

    // Content header and body frames of a message
    std::string contentFrames(const uint16_t channel, const std::string_view props, const std::string_view body, const uint32_t frameMax)
    {
        return contentHeaderFrame(channel, props, body.size()) + bodyFrames(channel, body, frameMax);
    }

    // Message properties relevant to the broker
    // Basic content header properties
    struct BasicProps
    {
        std::optional<std::string> contentType;
        std::optional<std::string> contentEncoding;
        std::optional<FieldTable> headers;
        std::optional<uint8_t> deliveryMode;
        std::optional<uint8_t> priority;
        std::optional<std::string> correlationId;
        std::optional<std::string> replyTo;
        std::optional<std::string> expiration;
        std::optional<std::string> messageId;
        std::optional<uint64_t> timestamp;
        std::optional<std::string> type;
        std::optional<std::string> userId;
        std::optional<std::string> appId;
        std::optional<std::string> clusterId;
    };

    // Parses property flags + properties of a content header
    bool parseBasicProps(const std::string_view raw, BasicProps& props)
    {
        Reader r(raw);
        uint16_t f = r.short_();
        // Continuation flags (bit 0) are not used by basic
        for (uint16_t cont = f; r.ok && (cont & 1);)
            cont = r.short_();
        if (!r.ok)
            return false;
        const auto has = [f](const int bit)
        {
            return (f >> bit) & 1;
        };
        if (has(15))
            props.contentType = r.shortstr();
        if (has(14))
            props.contentEncoding = r.shortstr();
        if (has(13))
            props.headers = r.table();
        if (has(12))
            props.deliveryMode = r.octet();
        if (has(11))
            props.priority = r.octet();
        if (has(10))
            props.correlationId = r.shortstr();
        if (has(9))
            props.replyTo = r.shortstr();
        if (has(8))
            props.expiration = r.shortstr();
        if (has(7))
            props.messageId = r.shortstr();
        if (has(6))
            props.timestamp = r.longlong();
        if (has(5))
            props.type = r.shortstr();
        if (has(4))
            props.userId = r.shortstr();
        if (has(3))
            props.appId = r.shortstr();
        if (has(2))
            props.clusterId = r.shortstr();
        return r.ok;
    }

    std::string serializeBasicProps(const BasicProps& props)
    {
        uint16_t f = 0;
        Writer w;
        const auto str = [&](const std::optional<std::string>& val, const int bit)
        {
            if (val)
            {
                f |= static_cast<uint16_t>(1 << bit);
                w.shortstr(*val);
            }
        };
        str(props.contentType, 15);
        str(props.contentEncoding, 14);
        if (props.headers)
        {
            f |= 1 << 13;
            w.table(*props.headers);
        }
        if (props.deliveryMode)
        {
            f |= 1 << 12;
            w.octet(*props.deliveryMode);
        }
        if (props.priority)
        {
            f |= 1 << 11;
            w.octet(*props.priority);
        }
        str(props.correlationId, 10);
        str(props.replyTo, 9);
        str(props.expiration, 8);
        str(props.messageId, 7);
        if (props.timestamp)
        {
            f |= 1 << 6;
            w.longlong(*props.timestamp);
        }
        str(props.type, 5);
        str(props.userId, 4);
        str(props.appId, 3);
        str(props.clusterId, 2);
        Writer ret;
        ret.short_(f);
        ret.out += w.out;
        return std::move(ret.out);
    }

    // Message properties relevant to the broker
    struct MessageProps
    {
        std::optional<int64_t> expirationMs;
        uint8_t deliveryMode = 0;
        uint8_t priority = 0;
    };

    bool parseProps(const std::string_view raw, MessageProps& props)
    {
        BasicProps basic;
        if (!parseBasicProps(raw, basic))
            return false;
        props.deliveryMode = basic.deliveryMode.value_or(0);
        props.priority = basic.priority.value_or(0);
        int64_t val;
        if (basic.expiration && parseInt64(*basic.expiration, val) && val >= 0)
            props.expirationMs = val;
        return true;
    }

    std::string serializeValue(const FieldValue& val)
    {
        Writer w;
        w.value(val);
        return std::move(w.out);
    }

    // Headers exchange: x-match all/any (all-with-x/any-with-x also compare x- headers). A binding argument of type
    // void only requires the header to be present
    bool headersMatch(const FieldTable& args, const FieldTable& headers)
    {
        std::string matchMode = "all";
        if (const auto* xm = tableGet(args, "x-match"))
            matchMode = xm->s;
        const bool any = matchMode == "any" || matchMode == "any-with-x";
        const bool withX = matchMode.ends_with("-with-x");
        bool matchedAny = false;
        for (const auto& [key, val] : args)
        {
            if (key == "x-match" || (!withX && key.starts_with("x-")))
                continue;
            const auto* header = tableGet(headers, key);
            const bool match = header != nullptr && (val.type == 'V' || serializeValue(*header) == serializeValue(val));
            if (any && match)
                return true;
            if (!any && !match)
                return false;
            matchedAny = matchedAny || match;
        }
        return !any;
    }

    // Topic exchange matching: words separated by '.', '*' matches one word, '#' zero or more
    bool topicMatch(const std::vector<std::string_view>& pattern, const size_t pi, const std::vector<std::string_view>& words, const size_t wi)
    {
        if (pi == pattern.size())
            return wi == words.size();
        if (pattern[pi] == "#")
        {
            for (size_t i = wi; i <= words.size(); ++i)
            {
                if (topicMatch(pattern, pi + 1, words, i))
                    return true;
            }
            return false;
        }
        if (wi == words.size())
            return false;
        if (pattern[pi] != "*" && pattern[pi] != words[wi])
            return false;
        return topicMatch(pattern, pi + 1, words, wi + 1);
    }

    std::vector<std::string_view> splitWords(const std::string_view str)
    {
        std::vector<std::string_view> ret;
        size_t start = 0;
        while (true)
        {
            const auto dot = str.find('.', start);
            ret.push_back(str.substr(start, dot == std::string_view::npos ? std::string_view::npos : dot - start));
            if (dot == std::string_view::npos)
                break;
            start = dot + 1;
        }
        return ret;
    }

    bool topicMatches(const std::string_view pattern, const std::string_view key)
    {
        if (pattern == "#")
            return true;
        const auto patternWords = splitWords(pattern);
        const auto keyWords = key.empty() ? std::vector<std::string_view>() : splitWords(key);
        return topicMatch(patternWords, 0, keyWords, 0);
    }

    std::string escapeVhost(const std::string_view vhost)
    {
        std::string ret;
        for (const char ch : vhost)
        {
            if (ch == '/' || ch == '%')
                ret += fmt::format("%{:02X}", static_cast<unsigned char>(ch));
            else
                ret.push_back(ch);
        }
        return ret;
    }
} // namespace

namespace
{
    std::string entityId(const std::string_view vhost, const std::string_view name)
    {
        std::string ret(vhost);
        ret.push_back('\0');
        ret.append(name);
        return ret;
    }

    std::string idStr(const uint64_t id)
    {
        return fmt::format("{:x}", id);
    }

    // Storage key of the messages of a queue
    std::string queueStorageKey(const uint64_t id)
    {
        return "q/" + idStr(id);
    }

    // Policy resource of a queue/exchange (by name)
    std::string queueResource(const std::string& vhost, const std::string& name)
    {
        return "q/" + escapeVhost(vhost) + "/" + name;
    }

    std::string exchangeResource(const std::string& vhost, const std::string& name)
    {
        return "e/" + escapeVhost(vhost) + "/" + name;
    }

    const std::string metaPrefix = "$m/";

    std::string exchangeMetaKey(const uint64_t id)
    {
        return metaPrefix + "e/" + idStr(id);
    }

    std::string queueMetaKey(const uint64_t id)
    {
        return metaPrefix + "q/" + idStr(id);
    }

    std::string bindingMetaKey(const uint64_t exchangeId, const uint64_t queueId, const std::string& routingKey)
    {
        return metaPrefix + "b/" + idStr(exchangeId) + "/" + idStr(queueId) + "/" + routingKey;
    }

    std::optional<uint64_t> parseId(const std::string_view str)
    {
        uint64_t id;
        const auto res = std::from_chars(str.data(), str.data() + str.size(), id, 16);
        if (res.ec != std::errc() || res.ptr != str.data() + str.size())
            return std::nullopt;
        return id;
    }

    std::optional<int64_t> argInt(const std::string& argsRaw, const std::string_view name)
    {
        if (argsRaw.empty())
            return std::nullopt;
        Reader r(argsRaw);
        const auto table = r.table();
        if (!r.ok)
            return std::nullopt;
        const auto* val = tableGet(table, name);
        return val ? val->asInt() : std::nullopt;
    }

    std::optional<std::string> argStr(const std::string& argsRaw, const std::string_view name)
    {
        if (argsRaw.empty())
            return std::nullopt;
        Reader r(argsRaw);
        const auto table = r.table();
        if (!r.ok)
            return std::nullopt;
        const auto* val = tableGet(table, name);
        if (val == nullptr || (val->type != 'S' && val->type != 'x'))
            return std::nullopt;
        return val->s;
    }

    std::string exchangeBindingMetaKey(const uint64_t sourceId, const uint64_t destId, const std::string& routingKey)
    {
        return metaPrefix + "x/" + idStr(sourceId) + "/" + idStr(destId) + "/" + routingKey;
    }

    void parseQueueArgs(AmqpServer::Queue& q)
    {
        q.expiresMs = argInt(q.argsRaw, "x-expires");
        q.messageTtlMs = argInt(q.argsRaw, "x-message-ttl");
        q.deadLetterExchange = argStr(q.argsRaw, "x-dead-letter-exchange");
        q.deadLetterRoutingKey = argStr(q.argsRaw, "x-dead-letter-routing-key");
    }

    int64_t steadyMs()
    {
        return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
    }

    // Inline metadata of a stored message: format version, exchange, routing key, properties, publish time
    std::string serializeMessageMeta(const std::string& exchange, const std::string& routingKey, const std::string& props, const int64_t publishMs)
    {
        CWData data;
        data.addVarInt(1);
        data.addString2(exchange);
        data.addString2(routingKey);
        data.addString2(props);
        data.addVarInt(publishMs);
        return std::string(data.getDataPtr(), data.getDataSize());
    }

    struct MessageMeta
    {
        std::string exchange;
        std::string routingKey;
        std::string props;
        int64_t publishMs = 0;
    };

    bool parseMessageMeta(const std::string& str, MessageMeta& meta)
    {
        CRData data(str.data(), str.size());
        int64_t format;
        return data.getVarInt(&format) && format == 1 && data.getStr2(&meta.exchange) && data.getStr2(&meta.routingKey) &&
               data.getStr2(&meta.props) && data.getVarInt(&meta.publishMs);
    }

    std::string serializeExchange(const AmqpServer::Exchange& ex)
    {
        CWData data;
        data.addVarInt(1);
        data.addString2(ex.vhost);
        data.addString2(ex.name);
        data.addString2(ex.type);
        data.addChar(ex.autoDelete ? 1 : 0);
        data.addChar(ex.internal ? 1 : 0);
        data.addString2(ex.argsRaw);
        return std::string(data.getDataPtr(), data.getDataSize());
    }

    std::string serializeQueue(const AmqpServer::Queue& q)
    {
        CWData data;
        data.addVarInt(1);
        data.addString2(q.vhost);
        data.addString2(q.name);
        data.addChar(q.autoDelete ? 1 : 0);
        data.addString2(q.argsRaw);
        return std::string(data.getDataPtr(), data.getDataSize());
    }

    std::string serializeBinding(const std::string& argsRaw)
    {
        CWData data;
        data.addVarInt(1);
        data.addString2(argsRaw);
        return std::string(data.getDataPtr(), data.getDataSize());
    }
} // namespace

// Connection of an AMQP client. Lives on one EventBase (evb), which reads and parses frames and writes output. Frames,
// deliveries and other protocol work run one after another on the serial executor of the connection, which owns the
// protocol state (channels etc.)
class AmqpConnection : public folly::AsyncTransport::ReadCallback,
                       public folly::AsyncTransport::WriteCallback,
                       public std::enable_shared_from_this<AmqpConnection>
{
public:
    using Queue = AmqpServer::Queue;
    using Consumer = AmqpServer::Consumer;
    using Msg = AmqpServer::Msg;
    using Error = AmqpServer::Error;

    AmqpConnection(AmqpServer& server, folly::EventBase* evb, folly::AsyncSocket::UniquePtr sock, const int64_t id)
        : server(server), evb(evb), serial(folly::SerialExecutor::create()), sock(std::move(sock)), id(id)
    {
    }

    // Role held while running on evb
    ThreadRole evbThread;
    // Role held by tasks on the serial executor, which own the protocol state
    ThreadRole serialThread;

    void assertOnEvb() const ASSERT_CAPABILITY(evbThread)
    {
        assert(evb->isInEventBaseThread());
    }

    void assertSerial() const ASSERT_CAPABILITY(serialThread)
    {
    }

    // Call on evb
    void start() REQUIRES(evbThread)
    {
        lastRead = lastWrite = std::chrono::steady_clock::now();
        heartbeatTimer = folly::AsyncTimeout::make(*evb, [this]() noexcept
                                                   {
                                                       assertOnEvb();
                                                       onHeartbeatTimer(); });
        heartbeatTimer->scheduleTimeout(std::chrono::seconds(1));
        sock->setReadCB(this);
    }

    // Call on evb. Closes the connection immediately
    void close() REQUIRES(evbThread)
    {
        if (closed)
            return;
        closed = true;
        closedFlag = true;
        auto self = shared_from_this();
        heartbeatTimer.reset();
        if (sock)
        {
            sock->setReadCB(nullptr);
            sock->closeNow();
            sock.reset();
        }
        // Requeue deliveries, remove consumers and exclusive queues
        runTask([self]()
                {
                    self->assertSerial();
                    self->cleanup();
                    self->server.connectionClosed(self->id);
                    self->server.removeConnection(self.get()); });
    }

    bool isClosed() const
    {
        return closedFlag;
    }

    size_t pendingOutput() const
    {
        return outputBytes.load() + deliveriesInFlight.load() * 1024;
    }

    // Delivers a message of queue to consumer (any thread). Counters were incremented by the dispatcher
    static void deliver(std::shared_ptr<AmqpConnection> conn, std::shared_ptr<Queue> queue, std::shared_ptr<Consumer> consumer, Msg msg)
    {
        ++conn->deliveriesInFlight;
        auto self = conn;
        self->runTask([conn = std::move(conn), queue = std::move(queue), consumer = std::move(consumer), msg]()
                      {
                          conn->assertSerial();
                          conn->doDeliver(queue, consumer, msg);
                          --conn->deliveriesInFlight; });
    }

    // Consumer was cancelled because its queue was deleted (any thread)
    static void consumerCancelled(std::shared_ptr<AmqpConnection> conn, std::shared_ptr<Consumer> consumer)
    {
        auto self = conn;
        self->runTask([conn = std::move(conn), consumer = std::move(consumer)]()
                      {
                          conn->assertSerial();
                          conn->onConsumerCancelled(consumer); });
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
        lastRead = std::chrono::steady_clock::now();
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
        XLOGF(DBG0, "AMQP connection {} read error: {}", id, ex.what());
        close();
    }

    void writeSuccess() noexcept override
    {
        assertOnEvb();
        if (writeSizes.empty())
            return;
        const auto size = writeSizes.front();
        const auto prev = outputBytes.fetch_sub(size);
        writeSizes.pop_front();
        // Resume deliveries blocked by the output limit
        if (prev >= maxPendingOutput && prev - size < maxPendingOutput)
        {
            auto self = shared_from_this();
            runTask([self]()
                    {
                        self->assertSerial();
                        self->redispatchAll(); });
        }
        // Continue a streamed message
        if (prev - size < outStreamHigh / 2 && waitingForDrain.exchange(false))
        {
            auto self = shared_from_this();
            runTask([self]()
                    {
                        self->assertSerial();
                        self->pump(); });
        }
    }

    void writeErr(size_t /*bytesWritten*/, const folly::AsyncSocketException& ex) noexcept override
    {
        assertOnEvb();
        XLOGF(DBG0, "AMQP connection {} write error: {}", id, ex.what());
        if (!writeSizes.empty())
        {
            outputBytes -= writeSizes.front();
            writeSizes.pop_front();
        }
        close();
    }

    folly::EventBase* getEventBase() const
    {
        return evb;
    }

private:
    struct Frame
    {
        uint8_t type;
        uint16_t channel;
        std::string payload;
    };

    struct Inflight
    {
        std::shared_ptr<Queue> queue;
        Msg msg;
        // Not set for basic.get
        std::shared_ptr<Consumer> consumer;
    };

    struct Channel
    {
        bool closing = false;
        std::shared_ptr<AmqpServer::ChannelCounters> counters = std::make_shared<AmqpServer::ChannelCounters>();
        // basic.qos for new consumers
        uint32_t prefetchCount = 0;
        uint64_t nextTag = 1;
        std::map<uint64_t, Inflight> inflight;
        std::map<std::string, std::pair<std::shared_ptr<Consumer>, std::shared_ptr<Queue>>> consumers;
        bool confirm = false;
        // tx.select: publishes and acks are applied on tx.commit
        bool tx = false;
        struct TxPublish
        {
            std::vector<std::shared_ptr<Queue>> targets;
            std::string meta;
            std::string body;
            // If the body was streamed: one per target
            std::vector<std::shared_ptr<StreamedValue>> streams;
            std::optional<int64_t> expirationMs;
            bool persistent = false;
        };
        std::vector<TxPublish> txPublishes;
        struct TxSettle
        {
            uint64_t tag;
            bool multiple;
            bool reject;
            bool requeue;
        };
        std::vector<TxSettle> txSettles;
        uint64_t publishSeq = 0;
        // Publish confirms to send after the next commit (seq, ok)
        std::vector<std::pair<uint64_t, bool>> pendingConfirms;
        // A pending confirm is for a persistent message (else confirms are sent without waiting for a commit)
        bool confirmsNeedCommit = false;
        // Confirms waiting for the running commit (sent when it finished)
        std::vector<std::pair<uint64_t, bool>> committingConfirms;

        // Message being received
        bool publishing = false;
        bool haveHeader = false;
        std::string pubExchange;
        std::string pubRoutingKey;
        bool pubMandatory = false;
        std::string pubProps;
        uint64_t pubBodySize = 0;
        uint64_t pubReceived = 0;
        // Routed when the content header is received
        std::vector<std::shared_ptr<Queue>> pubTargets;
        std::optional<int64_t> pubExpiration;
        // Delivery mode 2
        bool pubPersistent = false;
        // Large bodies are written to the data file while they are received (one stream per target queue)
        std::vector<std::shared_ptr<StreamedValue>> pubStreams;
        // Body of small messages (or unroutable mandatory ones, which are returned)
        bool pubKeepBody = false;
        std::string pubBody;
    };

    enum class State
    {
        AwaitHeader,
        AwaitStartOk,
        AwaitTuneOk,
        AwaitOpen,
        Open,
        // connection.close sent, waiting for close-ok
        Closing
    };

    template <typename F>
    void runTask(F f)
    {
        server.taskStarted();
        serial->add([self = shared_from_this(), f = std::move(f)]() mutable
                    {
                        try
                        {
                            f();
                        }
                        catch (const std::exception& e)
                        {
                            XLOGF(ERR, "Exception in AMQP connection {}: {}", self->id, e.what());
                        }
                        self->server.taskFinished(); });
    }

    // Writes data to the socket (any thread)
    // Output after a streamed message. Sent in order once the message was sent completely
    struct OutItem
    {
        std::string data;
        // Streamed message body (data is empty)
        std::shared_ptr<ObjectReader> reader;
        uint16_t channel = 0;
        int64_t size = 0;
        int64_t offset = 0;
    };

    // Sends data (in order with streamed messages)
    void send(std::string data) REQUIRES(serialThread)
    {
        if (data.empty())
            return;
        if (!outQueue.empty())
        {
            outQueue.push_back(OutItem{ .data = std::move(data) });
            return;
        }
        sendRaw(std::move(data));
    }

    // Sends queued output. Reads the next chunk of a streamed message only if not too much output is pending
    void pump() REQUIRES(serialThread)
    {
        while (!outQueue.empty() && !closedFlag)
        {
            auto& item = outQueue.front();
            if (!item.reader)
            {
                sendRaw(std::move(item.data));
                outQueue.pop_front();
                continue;
            }
            if (item.offset >= item.size)
            {
                outQueue.pop_front();
                continue;
            }
            if (outputBytes.load() >= outStreamHigh)
            {
                // Continued by writeSuccess
                waitingForDrain = true;
                return;
            }

            const auto len = std::min(outStreamChunk, item.size - item.offset);
            std::string chunk(static_cast<size_t>(len), '\0');
            try
            {
                item.reader->Read(chunk.data(), len, item.offset);
            }
            catch (const ObjectReadError& e)
            {
                // The message cannot be completed
                XLOGF(WARN, "AMQP connection {}: error reading streamed message: {}", id, e.what());
                closeNow();
                return;
            }
            item.offset += len;
            sendRaw(bodyFrames(item.channel, chunk, frameMax));
            if (item.offset < item.size)
            {
                // Let other tasks run between chunks
                auto self = shared_from_this();
                runTask([self]()
                        {
                            self->assertSerial();
                            self->pump(); });
                return;
            }
        }
    }

    // Writes data to the socket (any thread)
    void sendRaw(std::string data)
    {
        if (data.empty() || closedFlag)
            return;
        outputBytes += data.size();
        evb->runInEventBaseThread([self = shared_from_this(), data = std::move(data)]() mutable
                                  {
                                      self->assertOnEvb();
                                      if (!self->sock || self->closed)
                                      {
                                          self->outputBytes -= data.size();
                                          return;
                                      }
                                      self->lastWrite = std::chrono::steady_clock::now();
                                      self->writeSizes.push_back(data.size());
                                      self->sock->writeChain(self.get(), folly::IOBuf::fromString(std::move(data))); });
    }

    void parseInput() REQUIRES(evbThread)
    {
        size_t pos = 0;
        while (!closed)
        {
            const char* data = inbuf.data() + pos;
            const size_t avail = inLen - pos;
            if (!headerReceived)
            {
                if (avail < 8)
                    break;
                if (std::memcmp(data, "AMQP\0\0\x09\x01", 8) != 0)
                {
                    // Tell the client the supported version
                    if (sock)
                        sock->writeChain(nullptr, folly::IOBuf::copyBuffer("AMQP\0\0\x09\x01", 8));
                    close();
                    return;
                }
                headerReceived = true;
                pos += 8;
                auto self = shared_from_this();
                runTask([self]()
                        {
                            self->assertSerial();
                            self->sendStart(); });
                continue;
            }

            if (avail < 7)
                break;
            Reader r(data, 7);
            const auto type = r.octet();
            const auto channel = r.short_();
            const auto size = r.long_();
            if (size > std::max<uint32_t>(frameMax.load(), static_cast<uint32_t>(FLAGS_amqp_frame_max)))
            {
                XLOGF(INFO, "AMQP connection {}: frame too large ({} bytes)", id, size);
                close();
                return;
            }
            if (avail < 7 + static_cast<size_t>(size) + 1)
                break;
            if (static_cast<uint8_t>(data[7 + size]) != frameEnd)
            {
                XLOGF(INFO, "AMQP connection {}: invalid frame end", id);
                close();
                return;
            }
            if (type != frameHeartbeat)
            {
                pending.push_back(Frame{ .type = type, .channel = channel, .payload = std::string(data + 7, size) });
                pendingBytes += size;
            }
            pos += 7 + size + 1;
        }

        if (closed)
            return;

        if (pos > 0)
        {
            std::memmove(inbuf.data(), inbuf.data() + pos, inLen - pos);
            inLen -= pos;
        }
        if (inbuf.size() > 1024 * 1024 && inLen < inbuf.size() / 4)
        {
            inbuf.resize(std::max(inLen, static_cast<size_t>(64 * 1024)));
            inbuf.shrink_to_fit();
        }
        processPending();
    }

    void processPending() REQUIRES(evbThread)
    {
        if (closed)
            return;
        if (!processing && !pending.empty())
        {
            processing = true;
            std::vector<Frame> frames(std::make_move_iterator(pending.begin()), std::make_move_iterator(pending.end()));
            pending.clear();
            const auto bytes = pendingBytes;
            pendingBytes = 0;
            runTask([self = shared_from_this(), frames = std::move(frames), bytes]() mutable
                    {
                        self->assertSerial();
                        self->processFrames(frames);
                        self->evb->runInEventBaseThread([self, bytes]()
                                                        {
                                                            self->assertOnEvb();
                                                            self->processing = false;
                                                            self->updateReading();
                                                            self->processPending(); }); });
        }
        updateReading();
    }

    void updateReading() REQUIRES(evbThread)
    {
        if (closed || !sock)
            return;
        const bool pause = pendingBytes >= maxPendingInput;
        if (pause && !readPaused)
        {
            sock->setReadCB(nullptr);
            readPaused = true;
        }
        else if (!pause && readPaused)
        {
            readPaused = false;
            sock->setReadCB(this);
        }
    }

    void onHeartbeatTimer() REQUIRES(evbThread)
    {
        if (closed)
            return;
        const auto now = std::chrono::steady_clock::now();
        const auto hb = heartbeat.load();
        if (hb > 0)
        {
            if (now - lastRead > std::chrono::seconds(2 * hb))
            {
                XLOGF(INFO, "AMQP connection {}: missed heartbeats. Closing", id);
                close();
                return;
            }
            if (now - lastWrite >= std::chrono::seconds(std::max(1, hb / 2)) && sock)
            {
                lastWrite = now;
                outputBytes += 8;
                writeSizes.push_back(8);
                sock->writeChain(this, folly::IOBuf::fromString(frame(frameHeartbeat, 0, std::string_view())));
            }
        }
        heartbeatTimer->scheduleTimeout(std::chrono::seconds(1));
    }

    // ---- Protocol (on the serial executor) ----

    void sendStart() REQUIRES(serialThread)
    {
        FieldTable capabilities = { { "publisher_confirms", boolValue(true) },
                                    { "exchange_exchange_bindings", boolValue(true) },
                                    { "basic.nack", boolValue(true) },
                                    { "consumer_cancel_notify", boolValue(true) },
                                    { "connection.blocked", boolValue(false) },
                                    { "authentication_failure_close", boolValue(true) },
                                    { "per_consumer_qos", boolValue(true) } };
        FieldTable props = { { "product", stringValue("hs5") },
                             { "version", stringValue("1.0") },
                             { "platform", stringValue("C++") },
                             { "capabilities", tableValue(std::move(capabilities)) } };
        Writer w;
        w.octet(0).octet(9).table(props).longstr("PLAIN AMQPLAIN").longstr("en_US");
        send(methodFrame(0, classConnection, connectionStart, w.out));
        state = State::AwaitStartOk;
    }

    void connectionError(const uint16_t code, const std::string& text, const uint16_t classId, const uint16_t methodId) REQUIRES(serialThread)
    {
        XLOGF(INFO, "AMQP connection {} error {}: {}", id, code, text);
        Writer w;
        w.short_(code).shortstr(text).short_(classId).short_(methodId);
        send(methodFrame(0, classConnection, connectionClose, w.out));
        state = State::Closing;
        // Close if the client does not answer
        evb->runInEventBaseThread([self = shared_from_this()]()
                                  { self->evb->runAfterDelay([self]()
                                                             {
                                                                 self->assertOnEvb();
                                                                 self->close(); }, 2000); });
    }

    void closeNow()
    {
        evb->runInEventBaseThread([self = shared_from_this()]()
                                  {
                                      self->assertOnEvb();
                                      self->close(); });
    }

    void channelError(const uint16_t channelId, Channel& ch, const Error& err, const uint16_t classId, const uint16_t methodId) REQUIRES(serialThread)
    {
        XLOGF(DBG0, "AMQP connection {} channel {} error {}: {}", id, channelId, err.code, err.text);
        Writer w;
        w.short_(err.code).shortstr(err.text).short_(classId).short_(methodId);
        send(methodFrame(channelId, classChannel, channelClose, w.out));
        releaseChannel(ch);
        ch.closing = true;
    }

    // Requeues the deliveries and removes the consumers of a channel
    void releaseChannel(Channel& ch) REQUIRES(serialThread)
    {
        for (auto& [tag, entry] : ch.consumers)
        {
            entry.first->active = false;
            server.removeConsumer(entry.second, entry.first.get());
        }
        ch.consumers.clear();
        auto inflight = std::move(ch.inflight);
        ch.inflight.clear();
        for (auto& [tag, d] : inflight)
        {
            server.requeue(d.queue, d.msg, true);
            server.settled(d.queue, d.consumer.get(), ch.counters.get());
        }
        ch.publishing = false;
        ch.pubBody.clear();
    }

    void cleanup() REQUIRES(serialThread)
    {
        for (auto& [channelId, ch] : channels)
            releaseChannel(ch);
        channels.clear();
    }

    std::optional<Error> checkAuth(const std::string& key, const std::initializer_list<Action> actions) REQUIRES(serialThread)
    {
        const auto resource = fmt::format("arn:aws:s3:::{}/{}", server.store.bucketName, key);
        for (const auto action : actions)
        {
            if (!isAuthorized(resource, action, accessKey))
                return Error{ accessRefused, fmt::format("ACCESS_REFUSED - access to '{}' refused for user '{}'", key, accessKey) };
        }
        return std::nullopt;
    }

    std::optional<Error> checkQueueAuth(const std::string& queue, const std::initializer_list<Action> actions) REQUIRES(serialThread)
    {
        return checkAuth(queueResource(vhost, queue), actions);
    }

    std::optional<Error> checkExchangeAuth(const std::string& exchange, const std::initializer_list<Action> actions) REQUIRES(serialThread)
    {
        return checkAuth(exchangeResource(vhost, exchange), actions);
    }

    void processFrames(std::vector<Frame>& frames) REQUIRES(serialThread)
    {
        for (auto& f : frames)
        {
            if (closedFlag)
                return;
            processFrame(f);
        }
        finishConfirms();
    }

    // Sends the pending publish confirms, after a commit if a confirmed message is persistent. The commit runs in the
    // background (the connection keeps processing frames meanwhile). Only one commit runs at once per connection, so
    // confirms are sent in order; confirms of publishes during a commit are sent after the next one
    void finishConfirms() REQUIRES(serialThread)
    {
        if (confirmCommitRunning)
            return;

        bool any = false;
        bool needCommit = false;
        for (auto& [channelId, ch] : channels)
        {
            any = any || !ch.pendingConfirms.empty();
            needCommit = needCommit || ch.confirmsNeedCommit;
            ch.confirmsNeedCommit = false;
            // The messages were stored (queued to the storage) before, so a commit requested after this includes them
            ch.committingConfirms = std::move(ch.pendingConfirms);
            ch.pendingConfirms.clear();
        }
        if (!any)
            return;

        if (!needCommit)
        {
            // Non-persistent messages are confirmed once they are queued (like RabbitMQ)
            server.requestLazyCommit();
            sendCommittingConfirms(true);
            return;
        }

        confirmCommitRunning = true;
        server.taskStarted();
        folly::getGlobalCPUExecutor()->add([self = shared_from_this()]()
                                           {
                                               const bool ok = self->server.store.commit();
                                               self->runTask([self, ok]()
                                                             {
                                                                 self->assertSerial();
                                                                 self->confirmCommitRunning = false;
                                                                 self->sendCommittingConfirms(ok);
                                                                 // Publishes during the commit
                                                                 self->finishConfirms(); });
                                               self->server.taskFinished(); });
    }

    // Sends the confirms of committingConfirms (closed channels were removed with their confirms)
    void sendCommittingConfirms(const bool ok) REQUIRES(serialThread)
    {
        for (auto& [channelId, ch] : channels)
        {
            std::string out;
            // Coalesce acks (multiple)
            for (size_t i = 0; i < ch.committingConfirms.size(); ++i)
            {
                const auto [seq, msgOk] = ch.committingConfirms[i];
                const bool ack = ok && msgOk;
                const bool nextSame = i + 1 < ch.committingConfirms.size() && (ok && ch.committingConfirms[i + 1].second) == ack &&
                                      ch.committingConfirms[i + 1].first == seq + 1;
                if (nextSame && ack)
                    continue;
                Writer w;
                w.longlong(seq).bit(ack ? i > 0 : false);
                if (!ack)
                    w.bit(false);
                out += methodFrame(channelId, classBasic, ack ? basicAck : basicNack, w.out);
            }
            ch.committingConfirms.clear();
            send(std::move(out));
        }
    }

    void processFrame(Frame& f) REQUIRES(serialThread)
    {
        if (state == State::Closing)
        {
            // Only connection.close-ok (or close)
            if (f.type == frameMethod && f.channel == 0)
            {
                Reader r(f.payload);
                const auto classId = r.short_();
                const auto methodId = r.short_();
                if (classId == classConnection && methodId == connectionCloseOk)
                    closeNow();
                else if (classId == classConnection && methodId == connectionClose)
                {
                    send(methodFrame(0, classConnection, connectionCloseOk));
                    closeNow();
                }
            }
            return;
        }

        if (f.channel == 0)
        {
            if (f.type != frameMethod)
            {
                connectionError(unexpectedFrame, "UNEXPECTED_FRAME - expected method frame on channel 0", 0, 0);
                return;
            }
            Reader r(f.payload);
            const auto classId = r.short_();
            const auto methodId = r.short_();
            if (!r.ok || classId != classConnection)
            {
                connectionError(commandInvalid, "COMMAND_INVALID - expected connection method on channel 0", classId, methodId);
                return;
            }
            connectionMethod(methodId, r);
            return;
        }

        if (state != State::Open)
        {
            connectionError(unexpectedFrame, "UNEXPECTED_FRAME - connection not open", 0, 0);
            return;
        }

        auto it = channels.find(f.channel);
        if (f.type == frameMethod)
        {
            Reader r(f.payload);
            const auto classId = r.short_();
            const auto methodId = r.short_();
            if (!r.ok)
            {
                connectionError(frameError, "FRAME_ERROR - truncated method frame", 0, 0);
                return;
            }
            if (classId == classChannel && methodId == channelOpen)
            {
                // channel.open
                if (it != channels.end() || f.channel > maxChannels)
                {
                    connectionError(channelError_, "CHANNEL_ERROR - channel already open", classId, methodId);
                    return;
                }
                channels.emplace(f.channel, Channel());
                Writer w;
                w.longstr("");
                send(methodFrame(f.channel, classChannel, channelOpenOk, w.out));
                return;
            }
            if (it == channels.end())
            {
                connectionError(channelError_, "CHANNEL_ERROR - channel not open", classId, methodId);
                return;
            }
            auto& ch = it->second;
            if (ch.closing)
            {
                // Only channel.close-ok/close after a channel error
                if (classId == classChannel && (methodId == channelCloseOk || methodId == channelClose))
                {
                    if (methodId == channelClose)
                        send(methodFrame(f.channel, classChannel, channelCloseOk));
                    channels.erase(it);
                }
                return;
            }
            if (ch.publishing)
            {
                connectionError(unexpectedFrame, "UNEXPECTED_FRAME - expected content frame", classId, methodId);
                return;
            }
            channelMethod(f.channel, ch, classId, methodId, r);
            return;
        }

        if (it == channels.end())
        {
            connectionError(channelError_, "CHANNEL_ERROR - channel not open", 0, 0);
            return;
        }
        auto& ch = it->second;
        if (ch.closing)
            return;
        if (!ch.publishing)
        {
            connectionError(unexpectedFrame, "UNEXPECTED_FRAME - content frame without basic.publish", 0, 0);
            return;
        }

        if (f.type == frameHeader)
        {
            if (ch.haveHeader)
            {
                connectionError(unexpectedFrame, "UNEXPECTED_FRAME - duplicate content header", 0, 0);
                return;
            }
            Reader r(f.payload);
            r.short_(); // class
            r.short_(); // weight
            ch.pubBodySize = r.longlong();
            if (!r.ok)
            {
                connectionError(frameError, "FRAME_ERROR - truncated content header", 0, 0);
                return;
            }
            ch.pubProps = f.payload.substr(r.position());
            ch.haveHeader = true;
            if (static_cast<int64_t>(ch.pubBodySize) > FLAGS_amqp_max_message_size)
            {
                ch.publishing = false;
                channelError(f.channel, ch, Error{ contentTooLarge, fmt::format("CONTENT_TOO_LARGE - message size {} exceeds the maximum {}", ch.pubBodySize, FLAGS_amqp_max_message_size) },
                             classBasic, basicPublish);
                return;
            }
            ch.pubReceived = 0;
            ch.pubBody.clear();
            ch.pubStreams.clear();
            ch.pubTargets.clear();

            // Route now, so large bodies can be written to the target queues while they are received
            if (auto err = checkExchangeAuth(ch.pubExchange, { Action::PutObject }))
            {
                ch.publishing = false;
                return channelError(f.channel, ch, *err, classBasic, basicPublish);
            }
            if (auto err = server.route(vhost, ch.pubExchange, ch.pubRoutingKey, ch.pubProps, true, ch.pubTargets))
            {
                ch.publishing = false;
                return channelError(f.channel, ch, *err, classBasic, basicPublish);
            }
            MessageProps props;
            parseProps(ch.pubProps, props);
            ch.pubExpiration = props.expirationMs;
            ch.pubPersistent = props.deliveryMode == 2;

            if (!ch.pubTargets.empty() && ch.pubBodySize >= inStreamThreshold)
            {
                for (const auto& queue : ch.pubTargets)
                {
                    auto stream = std::make_shared<StreamedValue>(server.store.sfs, static_cast<size_t>(ch.pubBodySize), queue->key);
                    stream->assertWriter();
                    stream->prepare(server.store.bucketName);
                    ch.pubStreams.push_back(std::move(stream));
                }
            }
            ch.pubKeepBody = ch.pubStreams.empty() && (!ch.pubTargets.empty() || ch.pubMandatory);
            if (ch.pubKeepBody)
                ch.pubBody.reserve(static_cast<size_t>(ch.pubBodySize));
        }
        else if (f.type == frameBody)
        {
            if (!ch.haveHeader || ch.pubReceived + f.payload.size() > ch.pubBodySize)
            {
                connectionError(unexpectedFrame, "UNEXPECTED_FRAME - unexpected content body", 0, 0);
                return;
            }
            for (const auto& stream : ch.pubStreams)
            {
                stream->assertWriter();
                stream->write(static_cast<size_t>(ch.pubReceived), f.payload);
            }
            if (ch.pubKeepBody)
                ch.pubBody += f.payload;
            ch.pubReceived += f.payload.size();
        }
        else
        {
            connectionError(frameError, "FRAME_ERROR - unknown frame type", 0, 0);
            return;
        }

        if (ch.haveHeader && ch.pubReceived == ch.pubBodySize)
            completePublish(f.channel, ch);
    }

    static constexpr uint16_t channelError_ = 504;

    void connectionMethod(const uint16_t methodId, Reader& r) REQUIRES(serialThread)
    {
        switch (methodId)
        {
            case connectionStartOk: // start-ok
            {
                if (state != State::AwaitStartOk)
                    break;
                const auto clientProps = r.table();
                const auto mechanism = r.shortstr();
                const auto response = r.longstr();
                r.shortstr(); // locale
                if (!r.ok)
                {
                    connectionError(syntaxError, "SYNTAX_ERROR - invalid connection.start-ok", classConnection, connectionStartOk);
                    return;
                }
                if (const auto* caps = tableGet(clientProps, "capabilities"); caps && caps->table)
                {
                    if (const auto* ccn = tableGet(*caps->table, "consumer_cancel_notify"))
                        cancelNotify = ccn->b;
                }

                std::string user, password;
                if (mechanism == "PLAIN")
                {
                    const auto first = response.find('\0');
                    const auto second = first == std::string::npos ? std::string::npos : response.find('\0', first + 1);
                    if (second != std::string::npos)
                    {
                        user = response.substr(first + 1, second - first - 1);
                        password = response.substr(second + 1);
                    }
                }
                else if (mechanism == "AMQPLAIN")
                {
                    // Table without the length
                    Writer w;
                    w.longstr(response);
                    Reader tr(w.out);
                    const auto table = tr.table();
                    if (const auto* v = tableGet(table, "LOGIN"))
                        user = v->s;
                    if (const auto* v = tableGet(table, "PASSWORD"))
                        password = v->s;
                }
                else
                {
                    connectionError(accessRefused, fmt::format("ACCESS_REFUSED - unsupported mechanism '{}'", mechanism), classConnection, connectionStartOk);
                    return;
                }

                const auto secretKey = getSecretKey(user);
                if (user.empty() || secretKey.empty() || secretKey.size() != password.size() ||
                    sodium_memcmp(secretKey.data(), password.data(), password.size()) != 0)
                {
                    XLOGF(INFO, "AMQP connection {} authentication failed for access key {}", id, user);
                    connectionError(accessRefused, "ACCESS_REFUSED - Login was refused using authentication mechanism " + mechanism, classConnection, connectionStartOk);
                    return;
                }
                accessKey = user;

                Writer w;
                w.short_(maxChannels).long_(static_cast<uint32_t>(FLAGS_amqp_frame_max)).short_(static_cast<uint16_t>(FLAGS_amqp_heartbeat));
                send(methodFrame(0, classConnection, connectionTune, w.out));
                state = State::AwaitTuneOk;
                return;
            }
            case connectionTuneOk: // tune-ok
            {
                if (state != State::AwaitTuneOk)
                    break;
                r.short_(); // channel-max
                const auto fm = r.long_();
                const auto hb = r.short_();
                frameMax = fm == 0 ? static_cast<uint32_t>(FLAGS_amqp_frame_max) : std::min(fm, static_cast<uint32_t>(FLAGS_amqp_frame_max));
                frameMax = std::max(frameMax.load(), frameMinSize);
                heartbeat = hb;
                state = State::AwaitOpen;
                return;
            }
            case connectionOpen: // open
            {
                if (state != State::AwaitOpen)
                    break;
                vhost = r.shortstr();
                if (vhost.empty())
                    vhost = "/";
                Writer w;
                w.shortstr("");
                send(methodFrame(0, classConnection, connectionOpenOk, w.out));
                state = State::Open;
                return;
            }
            case connectionClose: // close
            {
                send(methodFrame(0, classConnection, connectionCloseOk));
                closeNow();
                return;
            }
            case connectionCloseOk: // close-ok
                closeNow();
                return;
            default:
                break;
        }
        connectionError(commandInvalid, fmt::format("COMMAND_INVALID - unexpected connection method {}", methodId), classConnection, methodId);
    }

    void channelMethod(const uint16_t channelId, Channel& ch, const uint16_t classId, const uint16_t methodId, Reader& r) REQUIRES(serialThread)
    {
        const auto fail = [&](const Error& err)
        {
            assertSerial();
            channelError(channelId, ch, err, classId, methodId);
        };
        const auto reply = [&](const uint16_t replyMethod, const std::string& args = std::string())
        {
            assertSerial();
            send(methodFrame(channelId, classId, replyMethod, args));
        };

        if (classId == classChannel)
        {
            switch (methodId)
            {
                case channelFlow: // flow
                {
                    const bool active = r.bit();
                    ch.counters->flowActive = active;
                    Writer w;
                    w.bit(active);
                    reply(channelFlowOk, w.out);
                    if (active)
                        redispatchChannel(ch);
                    return;
                }
                case channelFlowOk: // flow-ok
                    return;
                case channelClose: // close
                    releaseChannel(ch);
                    send(methodFrame(channelId, classChannel, channelCloseOk));
                    channels.erase(channelId);
                    return;
                default:
                    break;
            }
        }
        else if (classId == classExchange)
        {
            switch (methodId)
            {
                case exchangeDeclare: // declare
                {
                    r.short_();
                    const auto name = r.shortstr();
                    const auto type = r.shortstr();
                    const bool passive = r.bit(), durable = r.bit(), autoDelete = r.bit(), internal = r.bit(), noWait = r.bit();
                    r.endBits();
                    const auto args = r.rawTable();
                    if (!r.ok)
                        return fail(Error{ syntaxError, "SYNTAX_ERROR" });
                    if (auto err = checkExchangeAuth(name, { Action::PutObject }))
                        return fail(*err);
                    if (auto err = server.declareExchange(vhost, name, type, passive, durable, autoDelete, internal, args))
                        return fail(*err);
                    if (!noWait)
                        reply(exchangeDeclareOk);
                    return;
                }
                case exchangeBind:   // bind
                case exchangeUnbind: // unbind
                {
                    r.short_();
                    const auto destination = r.shortstr();
                    const auto source = r.shortstr();
                    const auto routingKey = r.shortstr();
                    const bool noWait = r.bit();
                    r.endBits();
                    const auto args = r.rawTable();
                    if (!r.ok)
                        return fail(Error{ syntaxError, "SYNTAX_ERROR" });
                    if (auto err = checkExchangeAuth(destination, { Action::PutObject }))
                        return fail(*err);
                    if (auto err = checkExchangeAuth(source, { Action::GetObject }))
                        return fail(*err);
                    if (auto err = server.bindExchange(vhost, destination, source, routingKey, args, methodId == exchangeBind))
                        return fail(*err);
                    if (!noWait)
                        reply(methodId == exchangeBind ? exchangeBindOk : exchangeUnbindOk);
                    return;
                }
                case exchangeDelete: // delete
                {
                    r.short_();
                    const auto name = r.shortstr();
                    const bool ifUnused = r.bit(), noWait = r.bit();
                    if (auto err = checkExchangeAuth(name, { Action::DeleteObject }))
                        return fail(*err);
                    if (auto err = server.deleteExchange(vhost, name, ifUnused))
                        return fail(*err);
                    if (!noWait)
                        reply(exchangeDeleteOk);
                    return;
                }
                default:
                    break;
            }
        }
        else if (classId == classQueue)
        {
            switch (methodId)
            {
                case queueDeclare: // declare
                {
                    r.short_();
                    auto name = r.shortstr();
                    const bool passive = r.bit(), durable = r.bit(), exclusive = r.bit(), autoDelete = r.bit(), noWait = r.bit();
                    r.endBits();
                    const auto args = r.rawTable();
                    if (!r.ok)
                        return fail(Error{ syntaxError, "SYNTAX_ERROR" });
                    if (!name.empty())
                    {
                        if (auto err = checkQueueAuth(name, { Action::PutObject }))
                            return fail(*err);
                    }
                    uint32_t messageCount = 0, consumerCount = 0;
                    if (auto err = server.declareQueue(id, vhost, name, passive, durable, exclusive, autoDelete, args, messageCount, consumerCount))
                        return fail(*err);
                    if (!noWait)
                    {
                        Writer w;
                        w.shortstr(name).long_(messageCount).long_(consumerCount);
                        reply(queueDeclareOk, w.out);
                    }
                    return;
                }
                case queueBind:   // bind
                case queueUnbind: // unbind
                {
                    r.short_();
                    const auto queue = r.shortstr();
                    const auto exchange = r.shortstr();
                    const auto routingKey = r.shortstr();
                    const bool noWait = methodId == queueBind ? r.bit() : false;
                    r.endBits();
                    const auto args = r.rawTable();
                    if (!r.ok)
                        return fail(Error{ syntaxError, "SYNTAX_ERROR" });
                    if (auto err = checkQueueAuth(queue, { Action::PutObject }))
                        return fail(*err);
                    if (auto err = server.bindQueue(id, vhost, queue, exchange, routingKey, args, methodId == queueBind))
                        return fail(*err);
                    if (!noWait)
                        reply(methodId == queueBind ? queueBindOk : queueUnbindOk);
                    return;
                }
                case queuePurge: // purge
                {
                    r.short_();
                    const auto queue = r.shortstr();
                    const bool noWait = r.bit();
                    if (auto err = checkQueueAuth(queue, { Action::DeleteObject }))
                        return fail(*err);
                    uint32_t count = 0;
                    if (auto err = server.purgeQueue(id, vhost, queue, count))
                        return fail(*err);
                    if (!noWait)
                    {
                        Writer w;
                        w.long_(count);
                        reply(queuePurgeOk, w.out);
                    }
                    return;
                }
                case queueDelete: // delete
                {
                    r.short_();
                    const auto queue = r.shortstr();
                    const bool ifUnused = r.bit(), ifEmpty = r.bit(), noWait = r.bit();
                    if (auto err = checkQueueAuth(queue, { Action::DeleteObject }))
                        return fail(*err);
                    uint32_t count = 0;
                    if (auto err = server.deleteQueue(id, vhost, queue, ifUnused, ifEmpty, count))
                        return fail(*err);
                    if (!noWait)
                    {
                        Writer w;
                        w.long_(count);
                        reply(queueDeleteOk, w.out);
                    }
                    return;
                }
                default:
                    break;
            }
        }
        else if (classId == classBasic)
        {
            switch (methodId)
            {
                case basicQos: // qos
                {
                    r.long_(); // prefetch-size (not supported, ignored like RabbitMQ)
                    const auto count = r.short_();
                    const bool global = r.bit();
                    if (global)
                        ch.counters->globalPrefetch = count;
                    else
                        ch.prefetchCount = count;
                    reply(basicQosOk);
                    redispatchChannel(ch);
                    return;
                }
                case basicConsume: // consume
                {
                    r.short_();
                    const auto queueName = r.shortstr();
                    auto tag = r.shortstr();
                    r.bit(); // no-local
                    const bool noAck = r.bit(), exclusive = r.bit(), noWait = r.bit();
                    r.endBits();
                    r.rawTable();
                    if (!r.ok)
                        return fail(Error{ syntaxError, "SYNTAX_ERROR" });
                    if (auto err = checkQueueAuth(queueName, { Action::GetObject, Action::DeleteObject }))
                        return fail(*err);
                    std::optional<Error> err;
                    auto queue = server.findQueue(id, vhost, queueName, err);
                    if (!queue)
                        return fail(*err);
                    if (tag.empty())
                        tag = fmt::format("amq.ctag-{}-{}", id, nextConsumerTag++);
                    if (ch.consumers.contains(tag))
                    {
                        connectionError(notAllowed, fmt::format("NOT_ALLOWED - attempt to reuse consumer tag '{}'", tag), classId, methodId);
                        return;
                    }
                    auto consumer = std::make_shared<Consumer>();
                    consumer->conn = weak_from_this();
                    consumer->channel = channelId;
                    consumer->tag = tag;
                    consumer->noAck = noAck;
                    consumer->counters = ch.counters;
                    consumer->prefetch = ch.prefetchCount;
                    {
                        std::scoped_lock qlock(queue->mutex);
                        if (queue->deleted)
                            return fail(Error{ notFound, fmt::format("NOT_FOUND - no queue '{}' in vhost '{}'", queueName, vhost) });
                        if (exclusive && !queue->consumers.empty())
                            return fail(Error{ accessRefused, fmt::format("ACCESS_REFUSED - queue '{}' in vhost '{}' in exclusive use", queueName, vhost) });
                        queue->consumers.push_back(consumer);
                        queue->hadConsumers = true;
                        queue->lastUsed = std::chrono::steady_clock::now();
                    }
                    ch.consumers[tag] = { consumer, queue };
                    if (!noWait)
                    {
                        Writer w;
                        w.shortstr(tag);
                        reply(basicConsumeOk, w.out);
                    }
                    server.dispatch(queue);
                    return;
                }
                case basicCancel: // cancel
                {
                    const auto tag = r.shortstr();
                    const bool noWait = r.bit();
                    auto it = ch.consumers.find(tag);
                    if (it != ch.consumers.end())
                    {
                        auto [consumer, queue] = it->second;
                        ch.consumers.erase(it);
                        consumer->active = false;
                        server.removeConsumer(queue, consumer.get());
                    }
                    if (!noWait)
                    {
                        Writer w;
                        w.shortstr(tag);
                        reply(basicCancelOk, w.out);
                    }
                    return;
                }
                case basicPublish: // publish
                {
                    r.short_();
                    ch.pubExchange = r.shortstr();
                    ch.pubRoutingKey = r.shortstr();
                    ch.pubMandatory = r.bit();
                    const bool immediate = r.bit();
                    if (!r.ok)
                        return fail(Error{ syntaxError, "SYNTAX_ERROR" });
                    if (immediate)
                        return fail(Error{ notImplemented, "NOT_IMPLEMENTED - immediate=true" });
                    ch.publishing = true;
                    ch.haveHeader = false;
                    ch.pubBody.clear();
                    if (ch.confirm)
                        ++ch.publishSeq;
                    return;
                }
                case basicGet: // get
                {
                    r.short_();
                    const auto queueName = r.shortstr();
                    const bool noAck = r.bit();
                    if (auto err = checkQueueAuth(queueName, { Action::GetObject, Action::DeleteObject }))
                        return fail(*err);
                    std::optional<Error> err;
                    auto queue = server.findQueue(id, vhost, queueName, err);
                    if (!queue)
                        return fail(*err);
                    handleBasicGet(channelId, ch, queue, noAck);
                    return;
                }
                case basicAck: // ack
                {
                    const auto tag = r.longlong();
                    const bool multiple = r.bit();
                    if (!settleOrQueue(ch, tag, multiple, false, false))
                        return fail(Error{ preconditionFailed, fmt::format("PRECONDITION_FAILED - unknown delivery tag {}", tag) });
                    return;
                }
                case basicReject: // reject
                {
                    const auto tag = r.longlong();
                    const bool requeue = r.bit();
                    if (!settleOrQueue(ch, tag, false, true, requeue))
                        return fail(Error{ preconditionFailed, fmt::format("PRECONDITION_FAILED - unknown delivery tag {}", tag) });
                    return;
                }
                case basicNack: // nack
                {
                    const auto tag = r.longlong();
                    const bool multiple = r.bit();
                    const bool requeue = r.bit();
                    if (!settleOrQueue(ch, tag, multiple, true, requeue))
                        return fail(Error{ preconditionFailed, fmt::format("PRECONDITION_FAILED - unknown delivery tag {}", tag) });
                    return;
                }
                case basicRecoverAsync: // recover-async
                case basicRecover:      // recover
                {
                    // Redelivered messages are requeued (also with requeue=false)
                    auto inflight = std::move(ch.inflight);
                    ch.inflight.clear();
                    for (auto& [tag, d] : inflight)
                    {
                        server.requeue(d.queue, d.msg, true);
                        server.settled(d.queue, d.consumer.get(), ch.counters.get());
                    }
                    if (methodId == basicRecover)
                        reply(basicRecoverOk);
                    return;
                }
                default:
                    break;
            }
        }
        else if (classId == classConfirm && methodId == confirmSelect)
        {
            const bool noWait = r.bit();
            if (ch.tx)
                return fail(Error{ preconditionFailed, "PRECONDITION_FAILED - cannot switch from tx to confirm mode" });
            ch.confirm = true;
            if (!noWait)
                reply(confirmSelectOk);
            return;
        }
        else if (classId == classTx)
        {
            switch (methodId)
            {
                case txSelect:
                    if (ch.confirm)
                        return fail(Error{ preconditionFailed, "PRECONDITION_FAILED - cannot switch from confirm to tx mode" });
                    ch.tx = true;
                    reply(txSelectOk);
                    return;
                case txCommit:
                {
                    if (!ch.tx)
                        return fail(Error{ preconditionFailed, "PRECONDITION_FAILED - channel is not transactional" });
                    auto publishes = std::move(ch.txPublishes);
                    ch.txPublishes.clear();
                    auto settles = std::move(ch.txSettles);
                    ch.txSettles.clear();
                    std::vector<std::shared_ptr<Queue>> dispatchQueues;
                    for (auto& pub : publishes)
                    {
                        std::shared_ptr<const AmqpServer::MemoryMessage> memory;
                        for (size_t i = 0; i < pub.targets.size(); ++i)
                        {
                            const auto& queue = pub.targets[i];
                            const auto rc = server.storeMessage(queue, pub.streams.empty() ? ValueArg{ .data = pub.body } : ValueArg{ .streamed = pub.streams[i] },
                                                                pub.meta, pub.expirationMs, pub.persistent, memory);
                            if (rc != 0)
                                return fail(Error{ internalError, fmt::format("INTERNAL_ERROR - storage error {}", rc) });
                            dispatchQueues.push_back(queue);
                        }
                    }
                    for (const auto& st : settles)
                    {
                        if (!settle(ch, st.tag, st.multiple, st.reject, st.requeue))
                            return fail(Error{ preconditionFailed, fmt::format("PRECONDITION_FAILED - unknown delivery tag {}", st.tag) });
                    }
                    if (!server.store.commit())
                        return fail(Error{ internalError, "INTERNAL_ERROR - storage commit failed" });
                    reply(txCommitOk);
                    for (const auto& queue : dispatchQueues)
                        server.dispatch(queue);
                    return;
                }
                case txRollback:
                    if (!ch.tx)
                        return fail(Error{ preconditionFailed, "PRECONDITION_FAILED - channel is not transactional" });
                    ch.txPublishes.clear();
                    ch.txSettles.clear();
                    reply(txRollbackOk);
                    return;
                default:
                    break;
            }
        }

        connectionError(commandInvalid, fmt::format("COMMAND_INVALID - unknown method {}.{}", classId, methodId), classId, methodId);
    }

    // Like settle, but only checks the tag and queues the settlement until tx.commit in transactions
    bool settleOrQueue(Channel& ch, const uint64_t tag, const bool multiple, const bool reject, const bool requeue) REQUIRES(serialThread)
    {
        if (!ch.tx)
            return settle(ch, tag, multiple, reject, requeue);
        const bool known = multiple ? (tag == 0 || (!ch.inflight.empty() && ch.inflight.begin()->first <= tag)) : ch.inflight.contains(tag);
        if (!known)
            return false;
        ch.txSettles.push_back(Channel::TxSettle{ .tag = tag, .multiple = multiple, .reject = reject, .requeue = requeue });
        return true;
    }

    // Acks or rejects deliveries. Returns false if the tag is unknown
    bool settle(Channel& ch, const uint64_t tag, const bool multiple, const bool reject, const bool requeue) REQUIRES(serialThread)
    {
        std::vector<Inflight> entries;
        if (multiple)
        {
            // tag 0: all
            const auto end = tag == 0 ? ch.inflight.end() : ch.inflight.upper_bound(tag);
            for (auto it = ch.inflight.begin(); it != end; ++it)
                entries.push_back(std::move(it->second));
            ch.inflight.erase(ch.inflight.begin(), end);
            if (entries.empty() && tag != 0)
                return false;
        }
        else
        {
            auto it = ch.inflight.find(tag);
            if (it == ch.inflight.end())
                return false;
            entries.push_back(std::move(it->second));
            ch.inflight.erase(it);
        }

        for (auto& d : entries)
        {
            if (reject && requeue)
                server.requeue(d.queue, d.msg, true);
            else
            {
                if (reject && d.queue->deadLetterExchange)
                    deadLetter(d.queue, d.msg, "rejected");
                server.deleteMessage(d.queue, d.msg);
            }
            server.settled(d.queue, d.consumer.get(), ch.counters.get());
        }
        // Also for persistent messages: after a crash, a message whose deletion was not committed yet is redelivered
        // (at-least-once, like RabbitMQ, which does not sync acks either). Dead lettered messages are committed by
        // deadLetter()
        if (!reject || !requeue)
            server.requestLazyCommit();
        return true;
    }

    struct OpenMessage
    {
        // Keeps the data readable (also after the message is deleted). Not set if the message is in memory
        std::shared_ptr<ObjectReader> reader;
        std::shared_ptr<const AmqpServer::MemoryMessage> memory;
        int64_t size = 0;
        MessageMeta meta;
    };

    // Opens a message (from memory if it is kept there and allowMemory, else from storage). Returns false if it was
    // deleted
    bool openMessage(const std::shared_ptr<Queue>& queue, const Msg& msg, OpenMessage& out, const bool allowMemory = true)
    {
        if (allowMemory && msg.memory)
        {
            out.memory = msg.memory;
            out.size = static_cast<int64_t>(msg.memory->body.size());
            if (!parseMessageMeta(msg.memory->meta, out.meta))
                out.meta = MessageMeta();
            return true;
        }
        try
        {
            out.reader = std::make_shared<ObjectReader>(server.store.sfs, server.store.bucketName, queue->key, msg.version, ObjectReader::VersionSelect::Exact);
            out.size = out.reader->FileSize();
            const auto objMetadata = out.reader->GetObjMetadata();
            if (objMetadata == nullptr || !objMetadata->redisData || !parseMessageMeta(*objMetadata->redisData, out.meta))
                out.meta = MessageMeta();
            return true;
        }
        catch (const ObjectReadError& e)
        {
            if (!e.notFound())
                XLOGF(WARN, "Error reading AMQP message of queue {}: {}", queue->name, e.what());
            return false;
        }
    }

    // Reads the complete body of an opened message
    std::optional<std::string> readBody(const OpenMessage& m)
    {
        if (m.memory)
            return m.memory->body;
        try
        {
            std::string body(static_cast<size_t>(m.size), '\0');
            if (m.size > 0)
                m.reader->Read(body.data(), m.size, 0);
            return body;
        }
        catch (const ObjectReadError& e)
        {
            XLOGF(WARN, "Error reading AMQP message body: {}", e.what());
            return std::nullopt;
        }
    }

    // Dead letters a message (reads it from storage)
    void deadLetter(const std::shared_ptr<Queue>& queue, const Msg& msg, const OpenMessage& m, const std::string& reason)
    {
        if (!queue->deadLetterExchange)
            return;
        if (m.reader)
        {
            server.deadLetter(queue, m.meta.exchange, m.meta.routingKey, m.meta.props, *m.reader, reason);
            return;
        }
        // In memory: the dead letter path copies from storage
        OpenMessage stored;
        if (openMessage(queue, msg, stored, false))
            server.deadLetter(queue, stored.meta.exchange, stored.meta.routingKey, stored.meta.props, *stored.reader, reason);
    }

    void deadLetter(const std::shared_ptr<Queue>& queue, const Msg& msg, const std::string& reason)
    {
        if (!queue->deadLetterExchange)
            return;
        OpenMessage m;
        if (openMessage(queue, msg, m, false))
            deadLetter(queue, msg, m, reason);
    }

    // Message expired (x-message-ttl of the queue or expiration property)
    static bool expired(const std::shared_ptr<Queue>& queue, const MessageMeta& meta)
    {
        MessageProps props;
        parseProps(meta.props, props);
        std::optional<int64_t> ttl = queue->messageTtlMs;
        if (props.expirationMs && (!ttl || *props.expirationMs < *ttl))
            ttl = props.expirationMs;
        return ttl && meta.publishMs > 0 && nowMs() - meta.publishMs >= *ttl;
    }

    // Sends a delivery (method frame) with the content of a message. Large bodies are streamed
    bool sendContent(const uint16_t channelId, std::string methodFrameData, const OpenMessage& m) REQUIRES(serialThread)
    {
        auto out = std::move(methodFrameData);
        out += contentHeaderFrame(channelId, m.meta.props, static_cast<uint64_t>(m.size));
        if (m.memory)
        {
            out += bodyFrames(channelId, m.memory->body, frameMax);
            send(std::move(out));
            return true;
        }
        if (m.size <= outStreamChunk)
        {
            auto body = readBody(m);
            if (!body)
                return false;
            out += bodyFrames(channelId, *body, frameMax);
            send(std::move(out));
            return true;
        }
        send(std::move(out));
        outQueue.push_back(OutItem{ .reader = m.reader, .channel = channelId, .size = m.size });
        // Otherwise a streamed message is being sent already (pump continues with this one)
        if (outQueue.size() == 1)
            pump();
        return true;
    }

    void doDeliver(const std::shared_ptr<Queue>& queue, const std::shared_ptr<Consumer>& consumer, Msg msg) REQUIRES(serialThread)
    {
        auto it = channels.find(consumer->channel);
        if (closedFlag || !consumer->active || it == channels.end() || it->second.closing)
        {
            server.requeue(queue, msg, false);
            if (!consumer->noAck)
                server.settled(queue, consumer.get(), consumer->counters.get());
            return;
        }
        auto& ch = it->second;

        OpenMessage m;
        const bool open = openMessage(queue, msg, m);
        if (!open || expired(queue, m.meta))
        {
            if (open)
                deadLetter(queue, msg, m, "expired");
            server.deleteMessage(queue, msg);
            if (!consumer->noAck)
                server.settled(queue, consumer.get(), consumer->counters.get());
            else
                server.dispatch(queue);
            return;
        }

        const auto tag = ch.nextTag++;
        ++msg.deliveryCount;
        Writer w;
        w.shortstr(consumer->tag).longlong(tag).bit(msg.redelivered).shortstr(m.meta.exchange).shortstr(m.meta.routingKey);
        if (!sendContent(consumer->channel, methodFrame(consumer->channel, classBasic, basicDeliver, w.out), m))
        {
            closeNow();
            return;
        }
        if (consumer->noAck)
        {
            // The reader keeps streamed data readable. Committed lazily (redelivered after a crash before the commit)
            server.deleteMessage(queue, msg);
            server.requestLazyCommit();
            server.dispatch(queue);
        }
        else
        {
            ch.inflight[tag] = Inflight{ .queue = queue, .msg = msg, .consumer = consumer };
        }
    }

    void handleBasicGet(const uint16_t channelId, Channel& ch, const std::shared_ptr<Queue>& queue, const bool noAck) REQUIRES(serialThread)
    {
        while (true)
        {
            std::optional<Msg> msg;
            uint32_t remaining = 0;
            {
                std::scoped_lock qlock(queue->mutex);
                queue->lastUsed = std::chrono::steady_clock::now();
                if (!queue->ready.empty())
                {
                    msg = queue->ready.front();
                    queue->ready.pop_front();
                    if (!noAck)
                        ++queue->unacked;
                }
                remaining = static_cast<uint32_t>(queue->ready.size());
            }
            if (!msg)
            {
                Writer w;
                w.shortstr("");
                send(methodFrame(channelId, classBasic, basicGetEmpty, w.out));
                return;
            }
            if (!noAck)
                ++ch.counters->unacked;

            OpenMessage m;
            const bool open = openMessage(queue, *msg, m);
            if (!open || expired(queue, m.meta))
            {
                if (open)
                    deadLetter(queue, *msg, m, "expired");
                server.deleteMessage(queue, *msg);
                if (!noAck)
                    server.settled(queue, nullptr, ch.counters.get());
                continue;
            }

            const auto tag = ch.nextTag++;
            ++msg->deliveryCount;
            Writer w;
            w.longlong(tag).bit(msg->redelivered).shortstr(m.meta.exchange).shortstr(m.meta.routingKey).long_(remaining);
            if (!sendContent(channelId, methodFrame(channelId, classBasic, basicGetOk, w.out), m))
            {
                closeNow();
                return;
            }
            if (noAck)
            {
                // Committed lazily (redelivered after a crash before the commit)
                server.deleteMessage(queue, *msg);
                server.requestLazyCommit();
            }
            else
            {
                ch.inflight[tag] = Inflight{ .queue = queue, .msg = *msg, .consumer = nullptr };
            }
            return;
        }
    }

    void completePublish(const uint16_t channelId, Channel& ch) REQUIRES(serialThread)
    {
        ch.publishing = false;
        ch.haveHeader = false;
        auto body = std::move(ch.pubBody);
        ch.pubBody = std::string();
        auto targets = std::move(ch.pubTargets);
        ch.pubTargets.clear();
        auto streams = std::move(ch.pubStreams);
        ch.pubStreams.clear();
        const auto seq = ch.publishSeq;
        for (const auto& stream : streams)
        {
            stream->assertWriter();
            stream->finish();
        }

        if (targets.empty())
        {
            if (ch.pubMandatory)
            {
                Writer w;
                w.short_(noRoute).shortstr("NO_ROUTE").shortstr(ch.pubExchange).shortstr(ch.pubRoutingKey);
                auto out = methodFrame(channelId, classBasic, basicReturn, w.out);
                out += contentFrames(channelId, ch.pubProps, body, frameMax);
                send(std::move(out));
            }
            if (ch.confirm)
                ch.pendingConfirms.emplace_back(seq, true);
            return;
        }

        const auto meta = serializeMessageMeta(ch.pubExchange, ch.pubRoutingKey, ch.pubProps, nowMs());
        if (ch.tx)
        {
            ch.txPublishes.push_back(Channel::TxPublish{ .targets = std::move(targets), .meta = meta, .body = std::move(body), .streams = std::move(streams), .expirationMs = ch.pubExpiration, .persistent = ch.pubPersistent });
            return;
        }
        bool ok = true;
        bool needCommit = false;
        std::shared_ptr<const AmqpServer::MemoryMessage> memory;
        for (size_t i = 0; i < targets.size(); ++i)
        {
            const auto rc = server.storeMessage(targets[i], streams.empty() ? ValueArg{ .data = body } : ValueArg{ .streamed = streams[i] }, meta, ch.pubExpiration,
                                                ch.pubPersistent, memory);
            if (rc != 0)
            {
                XLOGF(WARN, "Error storing AMQP message in queue {}: {}", targets[i]->name, rc);
                ok = false;
            }
            needCommit = needCommit || (ch.pubPersistent && targets[i]->durable);
        }
        if (ch.confirm)
        {
            ch.pendingConfirms.emplace_back(seq, ok);
            ch.confirmsNeedCommit = ch.confirmsNeedCommit || needCommit;
        }
        else if (needCommit)
            server.requestCommit();
        else
            server.requestLazyCommit();
        for (const auto& queue : targets)
            server.dispatch(queue);
    }

    void onConsumerCancelled(const std::shared_ptr<Consumer>& consumer) REQUIRES(serialThread)
    {
        auto it = channels.find(consumer->channel);
        if (it == channels.end())
            return;
        auto cit = it->second.consumers.find(consumer->tag);
        if (cit == it->second.consumers.end() || cit->second.first != consumer)
            return;
        it->second.consumers.erase(cit);
        if (cancelNotify)
        {
            Writer w;
            w.shortstr(consumer->tag).bit(true);
            send(methodFrame(consumer->channel, classBasic, basicCancel, w.out));
        }
    }

    void redispatchChannel(Channel& ch) REQUIRES(serialThread)
    {
        for (auto& [tag, entry] : ch.consumers)
        {
            entry.first->prefetch = ch.prefetchCount;
            server.dispatch(entry.second);
        }
    }

    void redispatchAll() REQUIRES(serialThread)
    {
        for (auto& [channelId, ch] : channels)
        {
            for (auto& [tag, entry] : ch.consumers)
                server.dispatch(entry.second);
        }
    }

    AmqpServer& server;
    folly::EventBase* const evb;
    const folly::Executor::KeepAlive<folly::SerialExecutor> serial;
    folly::AsyncSocket::UniquePtr sock GUARDED_BY(evbThread);
    const int64_t id;

    std::string inbuf GUARDED_BY(evbThread);
    size_t inLen GUARDED_BY(evbThread) = 0;
    // AMQP protocol header received
    bool headerReceived GUARDED_BY(evbThread) = false;
    // Frames not handed to the serial executor yet
    std::deque<Frame> pending GUARDED_BY(evbThread);
    size_t pendingBytes GUARDED_BY(evbThread) = 0;
    // Frames are being processed on the serial executor
    bool processing GUARDED_BY(evbThread) = false;
    bool readPaused GUARDED_BY(evbThread) = false;
    bool closed GUARDED_BY(evbThread) = false;
    // Sizes of the writes in progress (in order)
    std::deque<size_t> writeSizes GUARDED_BY(evbThread);
    std::chrono::steady_clock::time_point lastRead GUARDED_BY(evbThread);
    std::chrono::steady_clock::time_point lastWrite GUARDED_BY(evbThread);
    std::unique_ptr<folly::AsyncTimeout> heartbeatTimer GUARDED_BY(evbThread);

    // closed, for other threads
    std::atomic<bool> closedFlag{ false };
    // Data passed to send() and not written yet
    std::atomic<size_t> outputBytes{ 0 };
    // Deliveries queued on the serial executor
    std::atomic<size_t> deliveriesInFlight{ 0 };
    // Negotiated via connection.tune-ok
    std::atomic<uint32_t> frameMax{ frameMinSize };
    std::atomic<int> heartbeat{ 0 };

    State state GUARDED_BY(serialThread) = State::AwaitHeader;
    std::string accessKey GUARDED_BY(serialThread);
    std::string vhost GUARDED_BY(serialThread) = "/";
    // Client supports basic.cancel from the server
    bool cancelNotify GUARDED_BY(serialThread) = false;
    uint64_t nextConsumerTag GUARDED_BY(serialThread) = 1;
    std::map<uint16_t, Channel> channels GUARDED_BY(serialThread);
    // A commit for publish confirms is running (finishConfirms)
    bool confirmCommitRunning GUARDED_BY(serialThread) = false;
    // Output waiting for a streamed message to be sent (front is the streamed message)
    std::deque<OutItem> outQueue GUARDED_BY(serialThread);
    // pump() waits for output to be written
    std::atomic<bool> waitingForDrain{ false };
};

AmqpServer::AmqpServer(KeyStore& store)
    : store(store)
{
}

AmqpServer::~AmqpServer()
{
    stop();
}

void AmqpServer::taskStarted()
{
    std::scoped_lock lock(tasksMutex);
    ++runningTasks;
}

void AmqpServer::taskFinished()
{
    std::scoped_lock lock(tasksMutex);
    if (--runningTasks == 0)
        tasksCond.notify_all();
}

bool AmqpServer::storeDefinition(const std::string& key, const std::string& value)
{
    const auto rc = store.setValue(bucketId, key, ValueArg{ .data = value }, std::nullopt);
    if (rc != 0)
    {
        XLOGF(ERR, "Error storing AMQP definition {}: {}", key, rc);
        return false;
    }
    return store.commit();
}

bool AmqpServer::deleteDefinition(const std::string& key)
{
    bool existed;
    const auto rc = store.deleteKey(bucketId, key, existed);
    if (rc != 0)
    {
        XLOGF(ERR, "Error deleting AMQP definition {}: {}", key, rc);
        return false;
    }
    return store.commit();
}

bool AmqpServer::loadDefinitions()
{
    KeyStore::BucketCtx ctx;
    if (auto err = store.getBucketCtx(ctx))
    {
        XLOGF(ERR, "AMQP bucket error: {}", *err);
        return false;
    }
    bucketId = ctx.id;

    std::scoped_lock lock(mutex);

    // Predeclared exchanges (with reserved ids, so bindings to them can be stored)
    std::unordered_map<uint64_t, std::shared_ptr<Exchange>> exchangesById;
    uint64_t predeclaredId = std::numeric_limits<uint64_t>::max();
    for (const auto& [name, type] : std::vector<std::pair<std::string, std::string>>{
             { "", "direct" }, { "amq.direct", "direct" }, { "amq.fanout", "fanout" }, { "amq.topic", "topic" } })
    {
        auto ex = std::make_shared<Exchange>();
        ex->id = predeclaredId--;
        ex->vhost = "/";
        ex->name = name;
        ex->type = type;
        ex->durable = true;
        exchanges[entityId("/", name)] = ex;
        exchangesById[ex->id] = ex;
    }

    std::vector<std::string> keys;
    bool atEnd;
    if (!store.iterateKeys(bucketId, std::nullopt, metaPrefix, [&](const std::string& key, const KeyStore::KeyType type)
                           {
                               if (type == KeyStore::KeyType::String)
                                   keys.push_back(key);
                               return true; }, atEnd))
    {
        XLOGF(ERR, "Error listing AMQP definitions");
        return false;
    }

    std::unordered_map<uint64_t, std::shared_ptr<Queue>> queuesById;
    uint64_t maxId = 0;
    std::vector<std::pair<std::string, std::string>> bindings;
    for (const auto& key : keys)
    {
        const auto res = store.getString(key, KeyStore::ReadMode::Value);
        if (res.status != KeyStore::ReadRes::Status::Ok)
            continue;
        CRData data(res.value.data(), res.value.size());
        int64_t format;
        if (!data.getVarInt(&format) || format != 1)
            continue;

        if (key.starts_with(metaPrefix + "e/"))
        {
            const auto exId = parseId(std::string_view(key).substr(metaPrefix.size() + 2));
            auto ex = std::make_shared<Exchange>();
            char autoDelete, internal;
            if (!exId || !data.getStr2(&ex->vhost) || !data.getStr2(&ex->name) || !data.getStr2(&ex->type) || !data.getChar(&autoDelete) ||
                !data.getChar(&internal) || !data.getStr2(&ex->argsRaw))
                continue;
            ex->id = *exId;
            maxId = std::max(maxId, ex->id);
            ex->durable = true;
            ex->autoDelete = autoDelete != 0;
            ex->internal = internal != 0;
            exchanges[entityId(ex->vhost, ex->name)] = ex;
            exchangesById[ex->id] = ex;
        }
        else if (key.starts_with(metaPrefix + "q/"))
        {
            const auto qId = parseId(std::string_view(key).substr(metaPrefix.size() + 2));
            auto q = std::make_shared<Queue>();
            char autoDelete;
            if (!qId || !data.getStr2(&q->vhost) || !data.getStr2(&q->name) || !data.getChar(&autoDelete) || !data.getStr2(&q->argsRaw))
                continue;
            q->id = *qId;
            maxId = std::max(maxId, q->id);
            q->durable = true;
            q->autoDelete = autoDelete != 0;
            q->key = queueStorageKey(q->id);
            parseQueueArgs(*q);
            {
                std::scoped_lock qlock(q->mutex);
                q->lastUsed = std::chrono::steady_clock::now();
            }
            queues[entityId(q->vhost, q->name)] = q;
            queuesById[q->id] = q;
        }
        else if (key.starts_with(metaPrefix + "b/") || key.starts_with(metaPrefix + "x/"))
        {
            std::string argsRaw;
            data.getStr2(&argsRaw);
            bindings.emplace_back(key.substr(metaPrefix.size()), std::move(argsRaw));
        }
    }

    // b/<exchange id>/<queue id>/<routing key> or x/<source exchange id>/<destination exchange id>/<routing key>
    for (const auto& [typeAndRest, argsRaw] : bindings)
    {
        const bool toExchange = typeAndRest[0] == 'x';
        const auto rest = typeAndRest.substr(2);
        const auto slash1 = rest.find('/');
        const auto slash2 = slash1 == std::string::npos ? std::string::npos : rest.find('/', slash1 + 1);
        if (slash2 == std::string::npos)
            continue;
        const auto exId = parseId(std::string_view(rest).substr(0, slash1));
        const auto destId = parseId(std::string_view(rest).substr(slash1 + 1, slash2 - slash1 - 1));
        if (!exId || !destId || !exchangesById.contains(*exId))
            continue;
        std::string dest;
        if (toExchange)
        {
            auto it = exchangesById.find(*destId);
            if (it == exchangesById.end())
                continue;
            dest = entityId(it->second->vhost, it->second->name);
        }
        else
        {
            auto it = queuesById.find(*destId);
            if (it == queuesById.end())
                continue;
            dest = entityId(it->second->vhost, it->second->name);
        }
        exchangesById[*exId]->bindings.push_back(Binding{ .destId = dest, .toExchange = toExchange, .routingKey = rest.substr(slash2 + 1), .argsRaw = argsRaw });
    }

    // Messages of durable queues (oldest first). They may have been delivered before
    std::unordered_set<std::string> queueKeys;
    for (auto& [id, q] : queues)
    {
        queueKeys.insert(q->key);
        std::vector<KeyStore::Entry> entries;
        if (!store.listEntries(bucketId, q->key, entries))
        {
            XLOGF(ERR, "Error listing messages of AMQP queue {}", q->name);
            return false;
        }
        // Newest first
        if (!entries.empty())
            q->nextVersion = entries.front().version + 1;
        std::scoped_lock qlock(q->mutex);
        for (auto it = entries.rbegin(); it != entries.rend(); ++it)
        {
            if (!it->tombstone && it->isList)
                // Delivery mode is unknown (in the object metadata), stored messages of durable queues are persistent
                q->ready.push_back(Msg{ .version = it->version, .redelivered = true, .deliveryCount = 0, .persistent = true });
        }
    }

    // Messages of non-durable queues (from before the restart)
    // Each message is a version of the queue key. Stop after the first key and restart the listing after it,
    // so this seeks once per queue instead of stepping through all messages
    std::vector<std::string> staleKeys;
    std::optional<std::string> lastKey;
    while (true)
    {
        std::optional<std::string> currKey;
        if (!store.iterateKeys(bucketId, lastKey, "q/", [&](const std::string& key, const KeyStore::KeyType /*type*/)
                               {
                                   currKey = key;
                                   return false; }, atEnd))
        {
            return false;
        }
        if (!currKey)
            break;
        if (!queueKeys.contains(*currKey))
            staleKeys.push_back(*currKey);
        if (const auto qId = parseId(std::string_view(*currKey).substr(2)))
            maxId = std::max(maxId, *qId);
        lastKey = std::move(currKey);
    }
    nextId = maxId + 1;
    for (const auto& key : staleKeys)
    {
        bool existed;
        store.deleteKey(bucketId, key, existed);
    }
    if (!staleKeys.empty())
        store.commit();

    XLOGF(INFO, "Loaded {} AMQP exchanges, {} durable queues", exchanges.size(), queues.size());
    return true;
}

std::optional<AmqpServer::Error> AmqpServer::declareExchange(const std::string& vhost, const std::string& name, const std::string& type, const bool passive,
                                                             const bool durable, const bool autoDelete, const bool internal, const std::string& argsRaw)
{
    std::scoped_lock lock(mutex);
    const auto id = entityId(vhost, name);
    auto it = exchanges.find(id);
    if (it == exchanges.end() && vhost != "/" && name.empty())
    {
        // Default exchange of other vhosts
        auto ex = std::make_shared<Exchange>();
        ex->vhost = vhost;
        ex->type = "direct";
        ex->durable = true;
        it = exchanges.emplace(id, ex).first;
    }
    if (passive)
    {
        if (it == exchanges.end())
            return Error{ notFound, fmt::format("NOT_FOUND - no exchange '{}' in vhost '{}'", name, vhost) };
        return std::nullopt;
    }
    if (it != exchanges.end())
    {
        if (it->second->type != type)
            return Error{ preconditionFailed, fmt::format("PRECONDITION_FAILED - inequivalent arg 'type' for exchange '{}' in vhost '{}': received '{}' but current is '{}'",
                                                          name, vhost, type, it->second->type) };
        return std::nullopt;
    }
    if (name.starts_with("amq."))
        return Error{ accessRefused, fmt::format("ACCESS_REFUSED - exchange name '{}' contains reserved prefix 'amq.*'", name) };
    if (type != "direct" && type != "fanout" && type != "topic" && type != "headers")
        return Error{ commandInvalid, fmt::format("COMMAND_INVALID - unknown exchange type '{}'", type) };

    auto ex = std::make_shared<Exchange>();
    ex->vhost = vhost;
    ex->name = name;
    ex->type = type;
    ex->durable = durable;
    ex->autoDelete = autoDelete;
    ex->internal = internal;
    ex->argsRaw = argsRaw;
    ex->id = nextId++;
    if (durable && !storeDefinition(exchangeMetaKey(ex->id), serializeExchange(*ex)))
        return Error{ internalError, "INTERNAL_ERROR - storage error" };
    exchanges[id] = ex;
    return std::nullopt;
}

std::optional<AmqpServer::Error> AmqpServer::deleteExchange(const std::string& vhost, const std::string& name, const bool ifUnused)
{
    std::scoped_lock lock(mutex);
    auto it = exchanges.find(entityId(vhost, name));
    if (it == exchanges.end())
        return std::nullopt;
    if (name.empty() || name.starts_with("amq."))
        return Error{ accessRefused, fmt::format("ACCESS_REFUSED - operation not permitted on the default exchange") };
    if (ifUnused && !it->second->bindings.empty())
        return Error{ preconditionFailed, fmt::format("PRECONDITION_FAILED - exchange '{}' in vhost '{}' in use", name, vhost) };
    const auto deleted = it->second;
    if (deleted->durable)
    {
        for (const auto& binding : deleted->bindings)
        {
            if (binding.toExchange)
            {
                auto eit = exchanges.find(binding.destId);
                if (eit != exchanges.end() && eit->second->durable)
                    deleteDefinition(exchangeBindingMetaKey(deleted->id, eit->second->id, binding.routingKey));
                continue;
            }
            auto qit = queues.find(binding.destId);
            if (qit != queues.end() && qit->second->durable)
                deleteDefinition(bindingMetaKey(deleted->id, qit->second->id, binding.routingKey));
        }
        deleteDefinition(exchangeMetaKey(deleted->id));
    }
    exchanges.erase(it);

    // Bindings with the exchange as destination
    const auto deletedId = entityId(vhost, name);
    for (auto& [exId, ex] : exchanges)
    {
        std::erase_if(ex->bindings, [&](const Binding& b)
                      {
                          if (!b.toExchange || b.destId != deletedId)
                              return false;
                          if (ex->durable && deleted->durable)
                              deleteDefinition(exchangeBindingMetaKey(ex->id, deleted->id, b.routingKey));
                          return true; });
    }
    return std::nullopt;
}

std::optional<AmqpServer::Error> AmqpServer::declareQueue(const int64_t connId, const std::string& vhost, std::string& name, const bool passive,
                                                          const bool durable, const bool exclusive, const bool autoDelete, const std::string& argsRaw,
                                                          uint32_t& messageCount, uint32_t& consumerCount)
{
    std::shared_ptr<Queue> q;
    {
        std::scoped_lock lock(mutex);
        if (name.empty() && !passive)
        {
            do
            {
                name = fmt::format("amq.gen-{}", folly::hexlify(std::string_view(reinterpret_cast<const char*>(std::array<uint64_t, 2>{ folly::Random::rand64(), folly::Random::rand64() }.data()), 16)));
            } while (queues.contains(entityId(vhost, name)));
        }

        const auto id = entityId(vhost, name);
        auto it = queues.find(id);
        if (it == queues.end())
        {
            if (passive)
                return Error{ notFound, fmt::format("NOT_FOUND - no queue '{}' in vhost '{}'", name, vhost) };
            if (name.starts_with("amq.") && !name.starts_with("amq.gen-"))
                return Error{ accessRefused, fmt::format("ACCESS_REFUSED - queue name '{}' contains reserved prefix 'amq.*'", name) };

            q = std::make_shared<Queue>();
            q->vhost = vhost;
            q->name = name;
            q->id = nextId++;
            q->key = queueStorageKey(q->id);
            q->durable = durable && !exclusive;
            q->exclusive = exclusive;
            q->autoDelete = autoDelete;
            q->owner = exclusive ? connId : 0;
            q->argsRaw = argsRaw;
            parseQueueArgs(*q);
            {
                std::scoped_lock qlock(q->mutex);
                q->lastUsed = std::chrono::steady_clock::now();
            }
            if (q->durable && !storeDefinition(queueMetaKey(q->id), serializeQueue(*q)))
                return Error{ internalError, "INTERNAL_ERROR - storage error" };
            queues[id] = q;

            // Bound to the default exchange
            auto defaultEx = exchanges.find(entityId(vhost, ""));
            if (defaultEx == exchanges.end())
            {
                auto ex = std::make_shared<Exchange>();
                ex->vhost = vhost;
                ex->type = "direct";
                ex->durable = true;
                exchanges[entityId(vhost, "")] = ex;
            }
        }
        else
        {
            q = it->second;
            if (q->exclusive && q->owner != connId)
                return Error{ resourceLocked, fmt::format("RESOURCE_LOCKED - cannot obtain exclusive access to locked queue '{}' in vhost '{}'", name, vhost) };
        }
    }

    std::scoped_lock qlock(q->mutex);
    q->lastUsed = std::chrono::steady_clock::now();
    messageCount = static_cast<uint32_t>(q->ready.size());
    consumerCount = static_cast<uint32_t>(q->consumers.size());
    return std::nullopt;
}

std::shared_ptr<AmqpServer::Queue> AmqpServer::findQueue(const int64_t connId, const std::string& vhost, const std::string& name, std::optional<Error>& err)
{
    SharedLock lock(mutex);
    auto it = queues.find(entityId(vhost, name));
    if (it == queues.end())
    {
        err = Error{ notFound, fmt::format("NOT_FOUND - no queue '{}' in vhost '{}'", name, vhost) };
        return nullptr;
    }
    if (it->second->exclusive && it->second->owner != connId)
    {
        err = Error{ resourceLocked, fmt::format("RESOURCE_LOCKED - cannot obtain exclusive access to locked queue '{}' in vhost '{}'", name, vhost) };
        return nullptr;
    }
    return it->second;
}

void AmqpServer::removeQueue(const std::shared_ptr<Queue>& q)
{
    std::vector<std::shared_ptr<Consumer>> consumers;
    {
        std::scoped_lock lock(mutex);
        auto it = queues.find(entityId(q->vhost, q->name));
        if (it == queues.end() || it->second != q)
            return;
        queues.erase(it);

        // Bindings
        const auto qid = entityId(q->vhost, q->name);
        std::vector<std::string> emptyAutoDelete;
        for (auto& [exId, ex] : exchanges)
        {
            const auto removed = std::erase_if(ex->bindings, [&](const Binding& b)
                                               {
                                                   if (b.toExchange || b.destId != qid)
                                                       return false;
                                                   if (q->durable && ex->durable)
                                                       deleteDefinition(bindingMetaKey(ex->id, q->id, b.routingKey));
                                                   return true; });
            if (removed > 0 && ex->autoDelete && ex->bindings.empty())
                emptyAutoDelete.push_back(exId);
        }
        for (const auto& exId : emptyAutoDelete)
        {
            auto it2 = exchanges.find(exId);
            if (it2->second->durable)
                deleteDefinition(exchangeMetaKey(it2->second->id));
            exchanges.erase(it2);
        }

        if (q->durable)
            deleteDefinition(queueMetaKey(q->id));

        std::scoped_lock qlock(q->mutex);
        q->deleted = true;
        q->ready.clear();
        consumers = std::move(q->consumers);
        q->consumers.clear();
    }

    bool existed;
    store.deleteKey(bucketId, q->key, existed);
    requestCommit();

    for (auto& consumer : consumers)
        consumer->active = false;
    for (auto& consumer : consumers)
    {
        if (auto conn = consumer->conn.lock())
            AmqpConnection::consumerCancelled(conn, consumer);
    }
}

std::optional<AmqpServer::Error> AmqpServer::deleteQueue(const int64_t connId, const std::string& vhost, const std::string& name, const bool ifUnused,
                                                         const bool ifEmpty, uint32_t& messageCount)
{
    std::optional<Error> err;
    auto q = findQueue(connId, vhost, name, err);
    if (!q)
    {
        messageCount = 0;
        return err && err->code == notFound ? std::nullopt : err;
    }
    {
        std::scoped_lock qlock(q->mutex);
        if (ifUnused && !q->consumers.empty())
            return Error{ preconditionFailed, fmt::format("PRECONDITION_FAILED - queue '{}' in vhost '{}' in use", name, vhost) };
        if (ifEmpty && !q->ready.empty())
            return Error{ preconditionFailed, fmt::format("PRECONDITION_FAILED - queue '{}' in vhost '{}' is not empty", name, vhost) };
        messageCount = static_cast<uint32_t>(q->ready.size());
    }
    removeQueue(q);
    return std::nullopt;
}

std::optional<AmqpServer::Error> AmqpServer::bindQueue(const int64_t connId, const std::string& vhost, const std::string& queue, const std::string& exchange,
                                                       const std::string& routingKey, const std::string& argsRaw, const bool bind)
{
    std::optional<Error> err;
    auto q = findQueue(connId, vhost, queue, err);
    if (!q)
        return err;

    std::scoped_lock lock(mutex);
    auto it = exchanges.find(entityId(vhost, exchange));
    if (it == exchanges.end())
        return Error{ notFound, fmt::format("NOT_FOUND - no exchange '{}' in vhost '{}'", exchange, vhost) };
    auto& ex = *it->second;
    if (exchange.empty())
        return Error{ accessRefused, "ACCESS_REFUSED - operation not permitted on the default exchange" };

    const auto qid = entityId(vhost, queue);
    auto bit = std::find_if(ex.bindings.begin(), ex.bindings.end(), [&](const Binding& b)
                            { return !b.toExchange && b.destId == qid && b.routingKey == routingKey; });
    const bool persist = ex.durable && q->durable;
    if (bind)
    {
        if (bit != ex.bindings.end())
            return std::nullopt;
        if (persist && !storeDefinition(bindingMetaKey(ex.id, q->id, routingKey), serializeBinding(argsRaw)))
            return Error{ internalError, "INTERNAL_ERROR - storage error" };
        ex.bindings.push_back(Binding{ .destId = qid, .routingKey = routingKey, .argsRaw = argsRaw });
        return std::nullopt;
    }

    if (bit == ex.bindings.end())
        return std::nullopt;
    if (persist)
        deleteDefinition(bindingMetaKey(ex.id, q->id, routingKey));
    ex.bindings.erase(bit);
    if (ex.autoDelete && ex.bindings.empty())
    {
        if (ex.durable)
            deleteDefinition(exchangeMetaKey(ex.id));
        exchanges.erase(it);
    }
    return std::nullopt;
}

std::optional<AmqpServer::Error> AmqpServer::purgeQueue(const int64_t connId, const std::string& vhost, const std::string& name, uint32_t& messageCount)
{
    std::optional<Error> err;
    auto q = findQueue(connId, vhost, name, err);
    if (!q)
        return err;
    std::deque<Msg> purged;
    {
        std::scoped_lock qlock(q->mutex);
        purged = std::move(q->ready);
        q->ready.clear();
    }
    messageCount = static_cast<uint32_t>(purged.size());
    bool anyPersistent = false;
    for (const auto& msg : purged)
    {
        deleteMessage(q, msg);
        anyPersistent = anyPersistent || msg.persistent;
    }
    if (anyPersistent)
        requestCommit();
    else
        requestLazyCommit();
    return std::nullopt;
}

std::optional<AmqpServer::Error> AmqpServer::bindExchange(const std::string& vhost, const std::string& destination, const std::string& source,
                                                          const std::string& routingKey, const std::string& argsRaw, const bool bind)
{
    std::scoped_lock lock(mutex);
    auto sit = exchanges.find(entityId(vhost, source));
    if (sit == exchanges.end())
        return Error{ notFound, fmt::format("NOT_FOUND - no exchange '{}' in vhost '{}'", source, vhost) };
    auto dit = exchanges.find(entityId(vhost, destination));
    if (dit == exchanges.end())
        return Error{ notFound, fmt::format("NOT_FOUND - no exchange '{}' in vhost '{}'", destination, vhost) };
    if (source.empty() || destination.empty())
        return Error{ accessRefused, "ACCESS_REFUSED - operation not permitted on the default exchange" };

    auto& src = *sit->second;
    const auto& dst = *dit->second;
    const auto destId = entityId(vhost, destination);
    auto bit = std::find_if(src.bindings.begin(), src.bindings.end(), [&](const Binding& b)
                            { return b.toExchange && b.destId == destId && b.routingKey == routingKey; });
    const bool persist = src.durable && dst.durable;
    if (bind)
    {
        if (bit != src.bindings.end())
            return std::nullopt;
        if (persist && !storeDefinition(exchangeBindingMetaKey(src.id, dst.id, routingKey), serializeBinding(argsRaw)))
            return Error{ internalError, "INTERNAL_ERROR - storage error" };
        src.bindings.push_back(Binding{ .destId = destId, .toExchange = true, .routingKey = routingKey, .argsRaw = argsRaw });
        return std::nullopt;
    }

    if (bit == src.bindings.end())
        return std::nullopt;
    if (persist)
        deleteDefinition(exchangeBindingMetaKey(src.id, dst.id, routingKey));
    src.bindings.erase(bit);
    if (src.autoDelete && src.bindings.empty())
    {
        if (src.durable)
            deleteDefinition(exchangeMetaKey(src.id));
        exchanges.erase(sit);
    }
    return std::nullopt;
}

std::optional<AmqpServer::Error> AmqpServer::route(const std::string& vhost, const std::string& exchange, const std::string& routingKey,
                                                   const std::string& props, const bool fromClient, std::vector<std::shared_ptr<Queue>>& result)
{
    SharedLock lock(mutex);
    if (exchange.empty())
    {
        auto it = queues.find(entityId(vhost, routingKey));
        if (it != queues.end())
            result.push_back(it->second);
        return std::nullopt;
    }

    auto it = exchanges.find(entityId(vhost, exchange));
    if (it == exchanges.end())
        return Error{ notFound, fmt::format("NOT_FOUND - no exchange '{}' in vhost '{}'", exchange, vhost) };
    if (fromClient && it->second->internal)
        return Error{ accessRefused, fmt::format("ACCESS_REFUSED - cannot publish to internal exchange '{}' in vhost '{}'", exchange, vhost) };

    // Message headers (for headers exchanges), parsed when needed
    std::optional<FieldTable> headers;
    const auto getHeaders = [&]() -> const FieldTable&
    {
        if (!headers)
        {
            BasicProps basic;
            parseBasicProps(props, basic);
            headers = basic.headers.value_or(FieldTable());
        }
        return *headers;
    };

    // Follows exchange to exchange bindings (each exchange once)
    std::unordered_set<std::string> seenQueues;
    std::unordered_set<const Exchange*> seenExchanges;
    std::vector<const Exchange*> todo = { it->second.get() };
    seenExchanges.insert(it->second.get());
    while (!todo.empty())
    {
        const auto& ex = *todo.back();
        todo.pop_back();
        for (const auto& binding : ex.bindings)
        {
            bool match;
            if (ex.type == "fanout")
                match = true;
            else if (ex.type == "topic")
                match = topicMatches(binding.routingKey, routingKey);
            else if (ex.type == "headers")
            {
                Reader r(binding.argsRaw);
                const auto args = binding.argsRaw.empty() ? FieldTable() : r.table();
                match = headersMatch(args, getHeaders());
            }
            else
                match = binding.routingKey == routingKey;
            if (!match)
                continue;

            if (binding.toExchange)
            {
                auto eit = exchanges.find(binding.destId);
                if (eit != exchanges.end() && seenExchanges.insert(eit->second.get()).second)
                    todo.push_back(eit->second.get());
                continue;
            }
            if (!seenQueues.insert(binding.destId).second)
                continue;
            auto qit = queues.find(binding.destId);
            if (qit != queues.end())
                result.push_back(qit->second);
        }
    }
    return std::nullopt;
}

void AmqpServer::deadLetter(const std::shared_ptr<Queue>& queue, const std::string& exchange, const std::string& routingKey, const std::string& props,
                            ObjectReader& reader, const std::string& reason)
{
    if (!queue->deadLetterExchange)
        return;

    BasicProps basic;
    parseBasicProps(props, basic);
    auto headers = basic.headers.value_or(FieldTable());

    // x-death: one entry per queue and reason (newest first) with the number of times
    const auto timestamp = static_cast<int64_t>(std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count());
    FieldValue routingKeys;
    routingKeys.type = 'A';
    routingKeys.array.push_back(stringValue(routingKey));
    FieldTable death = { { "count", FieldValue{ .type = 'l', .i = 1 } },
                         { "reason", stringValue(reason) },
                         { "queue", stringValue(queue->name) },
                         { "time", FieldValue{ .type = 'T', .i = timestamp } },
                         { "exchange", stringValue(exchange) },
                         { "routing-keys", routingKeys } };
    if (basic.expiration)
        death.emplace_back("original-expiration", stringValue(*basic.expiration));

    FieldValue deaths;
    deaths.type = 'A';
    auto xdeath = std::find_if(headers.begin(), headers.end(), [](const auto& h)
                               { return h.first == "x-death"; });
    if (xdeath != headers.end() && xdeath->second.type == 'A')
    {
        for (auto& entry : xdeath->second.array)
        {
            if (entry.type != 'F' || !entry.table)
                continue;
            const auto* q = tableGet(*entry.table, "queue");
            const auto* r = tableGet(*entry.table, "reason");
            if (q && r && q->s == queue->name && r->s == reason)
            {
                // Same queue and reason: increment the count, move to the front
                for (auto& [k, v] : *entry.table)
                {
                    if (k == "count")
                        death[0].second.i = v.i + 1;
                }
                continue;
            }
            deaths.array.push_back(entry);
        }
        headers.erase(xdeath);
    }
    deaths.array.insert(deaths.array.begin(), tableValue(std::move(death)));
    headers.emplace_back("x-death", std::move(deaths));
    if (!tableGet(headers, "x-first-death-reason"))
    {
        headers.emplace_back("x-first-death-reason", stringValue(reason));
        headers.emplace_back("x-first-death-queue", stringValue(queue->name));
        headers.emplace_back("x-first-death-exchange", stringValue(exchange));
    }
    basic.headers = std::move(headers);
    // The dead lettered message must not expire again
    basic.expiration.reset();
    const auto newProps = serializeBasicProps(basic);

    const auto& dlx = *queue->deadLetterExchange;
    const auto dlrk = queue->deadLetterRoutingKey.value_or(routingKey);
    std::vector<std::shared_ptr<Queue>> targets;
    if (route(queue->vhost, dlx, dlrk, newProps, false, targets))
        return;
    if (targets.empty())
        return;
    const auto meta = serializeMessageMeta(dlx, dlrk, newProps, nowMs());

    try
    {
        const auto size = reader.FileSize();
        std::string body;
        std::vector<std::shared_ptr<StreamedValue>> streams;
        if (static_cast<uint64_t>(size) < inStreamThreshold)
        {
            body.resize(static_cast<size_t>(size));
            if (size > 0)
                reader.Read(body.data(), size, 0);
        }
        else
        {
            // Copy in chunks to the data file (one stream per target queue)
            for (const auto& target : targets)
            {
                auto stream = std::make_shared<StreamedValue>(store.sfs, static_cast<size_t>(size), target->key);
                stream->assertWriter();
                stream->prepare(store.bucketName);
                streams.push_back(std::move(stream));
            }
            std::string chunk;
            for (int64_t offset = 0; offset < size; offset += outStreamChunk)
            {
                const auto len = std::min(outStreamChunk, size - offset);
                chunk.resize(static_cast<size_t>(len));
                reader.Read(chunk.data(), len, offset);
                for (const auto& stream : streams)
                {
                    stream->assertWriter();
                    stream->write(static_cast<size_t>(offset), chunk);
                }
            }
            for (const auto& stream : streams)
            {
                stream->assertWriter();
                stream->finish();
            }
        }

        const bool persistent = basic.deliveryMode.value_or(0) == 2;
        bool needCommit = false;
        std::shared_ptr<const MemoryMessage> memory;
        for (size_t i = 0; i < targets.size(); ++i)
        {
            const auto rc = storeMessage(targets[i], streams.empty() ? ValueArg{ .data = body } : ValueArg{ .streamed = streams[i] }, meta, std::nullopt, persistent, memory);
            if (rc != 0)
                XLOGF(WARN, "Error storing dead lettered AMQP message in queue {}: {}", targets[i]->name, rc);
            needCommit = needCommit || (persistent && targets[i]->durable);
        }
        // The source message may have been non-persistent (its deletion is committed lazily)
        if (needCommit)
            requestCommit();
        else
            requestLazyCommit();
    }
    catch (const ObjectReadError& e)
    {
        XLOGF(WARN, "Error reading AMQP message of queue {} for dead lettering: {}", queue->name, e.what());
        return;
    }
    for (const auto& target : targets)
        dispatch(target);
}

void AmqpServer::connectionClosed(const int64_t connId)
{
    std::vector<std::shared_ptr<Queue>> exclusive;
    {
        SharedLock lock(mutex);
        for (const auto& [id, q] : queues)
        {
            if (q->exclusive && q->owner == connId)
                exclusive.push_back(q);
        }
    }
    for (const auto& q : exclusive)
        removeQueue(q);
}

AmqpServer::MemoryMessage::MemoryMessage(std::string body, std::string meta, std::atomic<int64_t>& memoryBytes)
    : body(std::move(body)), meta(std::move(meta)), memoryBytes(memoryBytes),
      // Including (approximately) the allocation overhead
      accounted(static_cast<int64_t>(this->body.size() + this->meta.size() + sizeof(MemoryMessage) + 64))
{
    memoryBytes.fetch_add(accounted, std::memory_order_relaxed);
}

AmqpServer::MemoryMessage::~MemoryMessage()
{
    memoryBytes.fetch_sub(accounted, std::memory_order_relaxed);
}

int AmqpServer::storeMessage(const std::shared_ptr<Queue>& queue, const ValueArg& value, const std::string& meta,
                             const std::optional<int64_t> expirationMs, const bool persistent, std::shared_ptr<const MemoryMessage>& memory)
{
    // Approximate limit (concurrent publishes may exceed it a bit)
    if (!memory && !value.streamed && static_cast<int64_t>(value.data.size()) <= FLAGS_amqp_memory_message_max &&
        memoryMessageBytes.load(std::memory_order_relaxed) + static_cast<int64_t>(value.data.size() + meta.size()) <= FLAGS_amqp_memory_max_mb * 1024 * 1024)
    {
        memory = std::make_shared<const MemoryMessage>(std::string(value.data), meta, memoryMessageBytes);
    }

    std::optional<int64_t> ttl = queue->messageTtlMs;
    if (expirationMs && (!ttl || *expirationMs < *ttl))
        ttl = expirationMs;
    const int64_t expiresAt = ttl ? nowMs() + *ttl : 0;

    KeyStore::PreparedValue prepared(store.sfs);
    auto rc = store.prepareValue(bucketId, queue->key, value, listContentType(), std::nullopt, prepared, &meta);
    if (rc != 0)
        return rc;
    const auto version = queue->nextVersion.fetch_add(1, std::memory_order_relaxed);
    rc = prepared.add(make_key(queue->key, bucketId, version), nullptr);
    if (rc != 0)
        return rc;

    {
        std::scoped_lock qlock(queue->mutex);
        if (queue->deleted)
        {
            bool deleted;
            store.deleteVersion(bucketId, queue->key, version, false, deleted);
            return 0;
        }
        queue->ready.push_back(Msg{ .version = version, .redelivered = false, .deliveryCount = 0, .expiresAt = expiresAt, .persistent = persistent && queue->durable, .memory = memory });
    }
    return 0;
}

void AmqpServer::deleteMessage(const std::shared_ptr<Queue>& queue, const Msg& msg)
{
    bool deleted;
    const auto rc = store.deleteVersion(bucketId, queue->key, msg.version, false, deleted);
    if (rc != 0 && rc != ENOENT)
        XLOGF(WARN, "Error deleting AMQP message of queue {}: {}", queue->name, rc);
}

void AmqpServer::requeue(const std::shared_ptr<Queue>& queue, Msg msg, const bool countAsDelivery)
{
    {
        std::scoped_lock qlock(queue->mutex);
        if (queue->deleted)
            return;
        msg.redelivered = msg.redelivered || countAsDelivery;
        // Before the messages which were not delivered yet (approximately the original position)
        queue->ready.push_front(msg);
    }
    dispatch(queue);
}

void AmqpServer::settled(const std::shared_ptr<Queue>& queue, Consumer* consumer, ChannelCounters* counters)
{
    if (consumer != nullptr && !consumer->noAck)
        --consumer->unacked;
    if (counters != nullptr)
        --counters->unacked;
    {
        std::scoped_lock qlock(queue->mutex);
        if (queue->unacked > 0)
            --queue->unacked;
    }
    dispatch(queue);
}

void AmqpServer::removeConsumer(const std::shared_ptr<Queue>& queue, const Consumer* consumer)
{
    bool deleteQueue = false;
    {
        std::scoped_lock qlock(queue->mutex);
        std::erase_if(queue->consumers, [consumer](const std::shared_ptr<Consumer>& c)
                      { return c.get() == consumer; });
        queue->lastUsed = std::chrono::steady_clock::now();
        deleteQueue = queue->autoDelete && queue->hadConsumers && queue->consumers.empty() && !queue->deleted;
    }
    if (deleteQueue)
        removeQueue(queue);
}

void AmqpServer::requestCommit()
{
    if (commitScheduled.exchange(true))
        return;
    taskStarted();
    folly::getGlobalCPUExecutor()->add([this]()
                                       {
                                           commitScheduled = false;
                                           // Includes the lazy changes
                                           lazyCommitPending = false;
                                           if (!store.commit())
                                               XLOGF(WARN, "AMQP background commit failed");
                                           taskFinished(); });
}

void AmqpServer::requestLazyCommit()
{
    lazyCommitPending.store(true, std::memory_order_relaxed);
}

void AmqpServer::expireMessages()
{
    std::vector<std::shared_ptr<Queue>> candidates;
    {
        SharedLock lock(mutex);
        for (const auto& [id, q] : queues)
            candidates.push_back(q);
    }

    const auto now = nowMs();
    bool any = false;
    bool anyPersistent = false;
    for (const auto& q : candidates)
    {
        // Only from the head (like RabbitMQ). Others expire when they reach the head or are delivered
        std::vector<Msg> expired;
        {
            std::scoped_lock qlock(q->mutex);
            while (!q->ready.empty() && q->ready.front().expiresAt != 0 && q->ready.front().expiresAt <= now)
            {
                expired.push_back(q->ready.front());
                q->ready.pop_front();
            }
        }
        for (const auto& msg : expired)
        {
            if (q->deadLetterExchange)
            {
                try
                {
                    ObjectReader reader(store.sfs, store.bucketName, q->key, msg.version, ObjectReader::VersionSelect::Exact);
                    MessageMeta meta;
                    const auto objMetadata = reader.GetObjMetadata();
                    if (objMetadata != nullptr && objMetadata->redisData && parseMessageMeta(*objMetadata->redisData, meta))
                        deadLetter(q, meta.exchange, meta.routingKey, meta.props, reader, "expired");
                }
                catch (const ObjectReadError& e)
                {
                    if (!e.notFound())
                        XLOGF(WARN, "Error reading expired AMQP message of queue {}: {}", q->name, e.what());
                }
            }
            deleteMessage(q, msg);
            any = true;
            anyPersistent = anyPersistent || msg.persistent;
        }
    }
    if (anyPersistent)
        requestCommit();
    else if (any)
        requestLazyCommit();
}

void AmqpServer::maintenanceThreadFunc()
{
    while (true)
    {
        {
            CondVarLock lock(maintenanceMutex);
            if (maintenanceStop)
                return;
            lock.waitFor(maintenanceCond, std::chrono::milliseconds(500));
            if (maintenanceStop)
                return;
        }

        std::vector<std::shared_ptr<Queue>> expired;
        {
            SharedLock lock(mutex);
            const auto now = std::chrono::steady_clock::now();
            for (const auto& [id, q] : queues)
            {
                if (!q->expiresMs)
                    continue;
                std::scoped_lock qlock(q->mutex);
                if (q->consumers.empty() && now - q->lastUsed > std::chrono::milliseconds(*q->expiresMs))
                    expired.push_back(q);
            }
        }
        for (const auto& q : expired)
        {
            XLOGF(DBG0, "AMQP queue {} expired", q->name);
            removeQueue(q);
        }

        expireMessages();

        if (lazyCommitPending.exchange(false))
            requestCommit();
    }
}

void AmqpServer::dispatch(const std::shared_ptr<Queue>& queue)
{
    struct Delivery
    {
        std::shared_ptr<AmqpConnection> conn;
        std::shared_ptr<Consumer> consumer;
        Msg msg;
    };
    std::vector<Delivery> deliveries;
    {
        std::scoped_lock qlock(queue->mutex);
        if (queue->deleted)
            return;
        while (!queue->ready.empty() && !queue->consumers.empty())
        {
            const auto n = queue->consumers.size();
            std::shared_ptr<Consumer> consumer;
            std::shared_ptr<AmqpConnection> conn;
            for (size_t k = 0; k < n; ++k)
            {
                const auto idx = (queue->nextConsumer + k) % n;
                auto& c = queue->consumers[idx];
                auto cconn = c->conn.lock();
                if (!cconn || cconn->isClosed() || !c->active || !c->counters->flowActive || cconn->pendingOutput() > maxPendingOutput)
                    continue;
                if (!c->noAck)
                {
                    const auto global = c->counters->globalPrefetch.load();
                    if ((c->prefetch > 0 && c->unacked >= c->prefetch) || (global > 0 && c->counters->unacked >= global))
                        continue;
                }
                consumer = c;
                conn = std::move(cconn);
                queue->nextConsumer = idx + 1;
                break;
            }
            if (!consumer)
                break;

            auto msg = queue->ready.front();
            queue->ready.pop_front();
            if (!consumer->noAck)
            {
                ++consumer->unacked;
                ++consumer->counters->unacked;
                ++queue->unacked;
            }
            deliveries.push_back(Delivery{ .conn = std::move(conn), .consumer = std::move(consumer), .msg = msg });
        }
    }

    for (auto& d : deliveries)
        AmqpConnection::deliver(std::move(d.conn), queue, std::move(d.consumer), d.msg);
}

void AmqpServer::Acceptor::connectionAccepted(folly::NetworkSocket fd, const folly::SocketAddress& clientAddr, AcceptInfo /*info*/) noexcept
{
    server.onAccepted(evb, fd, clientAddr);
}

void AmqpServer::Acceptor::acceptError(folly::exception_wrapper ew) noexcept
{
    XLOGF(WARN, "Error accepting AMQP connection: {}", ew.what());
}

void AmqpServer::onAccepted(folly::EventBase* evb, folly::NetworkSocket fd, const folly::SocketAddress& clientAddr)
{
    auto sock = folly::AsyncSocket::newSocket(evb, fd, &clientAddr);
    sock->setNoDelay(true);
    const auto connId = nextConnectionId++;
    auto conn = std::make_shared<AmqpConnection>(*this, evb, std::move(sock), connId);
    {
        std::scoped_lock lock(connectionsMutex);
        if (stopping)
            return;
        connections[conn.get()] = conn;
    }
    XLOGF(DBG0, "AMQP connection {} from {}", connId, clientAddr.describe());
    conn->assertOnEvb();
    conn->start();
}

void AmqpServer::removeConnection(AmqpConnection* conn)
{
    std::shared_ptr<AmqpConnection> removed;
    std::scoped_lock lock(connectionsMutex);
    auto it = connections.find(conn);
    if (it != connections.end())
    {
        removed = std::move(it->second);
        connections.erase(it);
    }
}

bool AmqpServer::start(const std::string& ip, int port, size_t ioThreads)
{
    if (!loadDefinitions())
        return false;

    ioPool = std::make_unique<folly::IOThreadPoolExecutor>(std::max(ioThreads, static_cast<size_t>(1)), std::make_shared<folly::NamedThreadFactory>("amqp io"));
    for (auto& evb : ioPool->getAllEventBases())
        evbs.push_back(evb.get());

    bool ok = false;
    evbs[0]->runInEventBaseThreadAndWait([&]()
                                         {
        try
        {
            serverSocket = folly::AsyncServerSocket::newSocket(evbs[0]);
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
            XLOGF(ERR, "Error starting AMQP server on {}:{}: {}", ip, port, e.what());
            serverSocket.reset();
        } });

    if (ok)
    {
        XLOGF(INFO, "AMQP server listening on {}:{} (bucket {})", ip, port, store.bucketName);
        maintenanceThread = std::thread([this]()
                                        { maintenanceThreadFunc(); });
    }
    return ok;
}

void AmqpServer::stop()
{
    if (!ioPool)
        return;

    if (maintenanceThread.joinable())
    {
        {
            std::scoped_lock lock(maintenanceMutex);
            maintenanceStop = true;
        }
        maintenanceCond.notify_all();
        maintenanceThread.join();
    }

    evbs[0]->runInEventBaseThreadAndWait([&]()
                                         {
        if (serverSocket)
        {
            serverSocket->stopAccepting();
            serverSocket.reset();
        } });

    std::vector<std::shared_ptr<AmqpConnection>> conns;
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

    store.commit();

    ioPool->stop();
    ioPool->join();
    ioPool.reset();
    acceptors.clear();
    evbs.clear();
}
