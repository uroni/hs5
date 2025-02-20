/**
 * Copyright Martin Raiber. All Rights Reserved.
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */
#include "utils.h"
#include <folly/Random.h>
#include <folly/String.h>

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