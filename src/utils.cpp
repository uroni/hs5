/**
 * Copyright Martin Raiber. All Rights Reserved.
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */
#include "utils.h"
#include <folly/Random.h>
#include <folly/String.h>
#include <folly/base64.h>

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