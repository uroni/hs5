/*
 * zig's libc (libzigc) implements strdup, strndup, wcsdup and reallocarray in
 * Zig. Their allocations are bound to libzigc's own allocator, not to the
 * (overridable) malloc symbol, so with jemalloc as malloc the results cannot be
 * passed to free()/realloc() (e.g. LMDB frees a strdup'ed path in mdb_env_close,
 * which crashed randomly). libzigc exports these as weak symbols, so these strong
 * definitions replace them. cmake/check-zig-libc.sh makes sure this list is
 * complete for the zig version used.
 */
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#if defined(__linux__) && !defined(__GLIBC__)

void* reallocarray(void* ptr, size_t nmemb, size_t size)
{
    if (size && nmemb > SIZE_MAX / size)
    {
        errno = ENOMEM;
        return NULL;
    }
    return realloc(ptr, nmemb * size);
}

char* strdup(const char* s)
{
    size_t len = strlen(s) + 1;
    char* d = malloc(len);
    if (d)
        memcpy(d, s, len);
    return d;
}

char* strndup(const char* s, size_t n)
{
    size_t len = strnlen(s, n);
    char* d = malloc(len + 1);
    if (d)
    {
        memcpy(d, s, len);
        d[len] = 0;
    }
    return d;
}

wchar_t* wcsdup(const wchar_t* s)
{
    size_t len = wcslen(s) + 1;
    wchar_t* d = malloc(len * sizeof(wchar_t));
    if (d)
        wmemcpy(d, s, len);
    return d;
}

#endif
