/**
 * Copyright Martin Raiber. All Rights Reserved.
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */
#include "utils.h"
#include <folly/Random.h>
#include <folly/String.h>
#include <folly/base64.h>
#include <sstream>

std::string random_uuid()
{
	std::string rnd;
	rnd.resize(16);
	folly::Random::secureRandom(&rnd[0], 16);
	return folly::hexlify(rnd);
}

std::string random_uuid_binary()
{
    std::string rnd;
	rnd.resize(16);
	folly::Random::secureRandom(&rnd[0], 16);
    return rnd;
}

std::string escapeXML(const std::string_view xml)
{
	std::string ret;
	ret.reserve(xml.size());
	for (size_t i = 0; i < xml.size(); ++i)
	{
		if (xml[i] == '<') ret += "&lt;";
		else if (xml[i] == '>') ret += "&gt;";
		else if (xml[i] == '&') ret += "&amp;";
		else if (xml[i] == '\"') ret += "&quot;";
		else ret += xml[i];
	}
	return ret;
}

std::string rfc2047EncodeB(std::string_view text, std::string_view charset) {
    bool needsEncoding = false;
    for (unsigned char ch : text) {
        if (ch < 32 || ch >= 127) {
            needsEncoding = true;
            break;
        }
    }

    if (!needsEncoding) {
        return std::string(text);
    }

    std::string b64 = folly::base64Encode(text);
    return "=?"+std::string(charset)+"?B?"+b64+"?=";
}

std::string rfc2047Decode(const std::string_view text)
{
	//TODO: Implement
	return std::string(text);
}

std::string asciiToLower(std::string str)
{
    std::transform(str.begin(), str.end(), str.begin(),
        [](auto c) { return std::tolower(c); });
    return str;
}

std::string asciiToLower(const std::string_view str)
{
	std::string ret;
	ret.reserve(str.size());
	for (char c : str)
		ret += std::tolower(c);
	return ret;
}

std::string format_last_modified(const int64_t lastModified)
{
    constexpr auto epoch = std::chrono::time_point<std::chrono::system_clock>();
    return format_last_modified(std::chrono::time_point_cast<std::chrono::system_clock::duration>(epoch + std::chrono::nanoseconds(lastModified)));
}

std::string format_last_modified(const std::chrono::system_clock::time_point lastModified)
{
    const auto lastModifiedTp = std::chrono::system_clock::to_time_t(lastModified);

    char datebuf[sizeof("2011-10-08T07:07:09Z")];
    std::strftime(datebuf, sizeof(datebuf), "%FT%TZ", std::gmtime(&lastModifiedTp));
    return datebuf;
}

std::string format_last_modified_rfc1123(const int64_t lastModified)
{
    constexpr auto epoch = std::chrono::time_point<std::chrono::system_clock>();
    const auto lastModifiedTp = std::chrono::system_clock::to_time_t(epoch + std::chrono::nanoseconds(lastModified));

    char datebuf[sizeof("Fri, 08 Oct 2011 07:07:09 GMT")];
    std::strftime(datebuf, sizeof(datebuf), "%a, %d %b %Y %H:%M:%S GMT", std::gmtime(&lastModifiedTp));
    return datebuf;
}

std::string format_last_modified_rfc1123(const std::chrono::system_clock::time_point lastModified)
{
    const auto lastModifiedTp = std::chrono::system_clock::to_time_t(lastModified);

    char datebuf[sizeof("Fri, 08 Oct 2011 07:07:09 GMT")];
    std::strftime(datebuf, sizeof(datebuf), "%a, %d %b %Y %H:%M:%S GMT", std::gmtime(&lastModifiedTp));
    return datebuf;
}

std::optional<std::chrono::system_clock::time_point> parse_iso8601(const std::string_view str)
{
    for(const auto fmt: {"%FT%T%Ez", "%FT%TZ"})
    {
        std::istringstream in{std::string(str)};
        std::chrono::sys_time<std::chrono::microseconds> tp;
        if(in >> std::chrono::parse(fmt, tp) && in.peek() == EOF)
            return tp;
    }
    return std::nullopt;
}
