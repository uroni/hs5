/**
 * Copyright Martin Raiber. All Rights Reserved.
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */
#include "File.h"

char *zeroBuf()
{
    static char *zeroes = static_cast<char *>(calloc(fillPageSize, 1));
    return zeroes;
}
