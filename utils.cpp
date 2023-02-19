/**
 * Copyright Martin Raiber. All Rights Reserved.
 * SPDX-License-Identifier: AGPL-3.0-or-later
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