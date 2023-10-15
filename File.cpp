#include "File.h"

char *zeroBuf()
{
    static char *zeroes = static_cast<char *>(calloc(fillPageSize, 1));
    return zeroes;
}
