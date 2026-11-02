#!/bin/bash
# zig's libc (libzigc) implements some libc functions in Zig. Allocations made
# inside libzigc are bound to its own allocator, not to the malloc symbol that
# hs5 replaces with jemalloc, so memory returned by such functions must not be
# freed with free(). src/zig_libc_compat.c overrides the affected functions.
# This check fails if the zig version used has affected functions that are not
# overridden, or allocator functions that jemalloc does not replace.

set -euo pipefail

ZIG="${ZIG:-zig}"
LIB_DIR="$("$ZIG" env | sed -n 's/^ *\.lib_dir = "\(.*\)",$/\1/p')"
LIBC_DIR="$LIB_DIR/c"

if [[ ! -d "$LIBC_DIR" ]]; then
  echo "check-zig-libc: Cannot find zig libc sources at '$LIBC_DIR'" >&2
  exit 1
fi

# Functions overridden in src/zig_libc_compat.c
OVERRIDDEN="reallocarray strdup strndup wcsdup"
# Allocator functions libzigc exports that jemalloc replaces (plus the overridden reallocarray)
ALLOCATOR="aligned_alloc calloc free malloc malloc_usable_size memalign posix_memalign realloc reallocarray valloc"

# Functions (outside of the allocator itself) that allocate or call an allocating function
ALLOCATING_CALL='(^|[^A-Za-z0-9_])(std\.)?c\.(malloc|calloc|realloc|reallocarray|aligned_alloc|posix_memalign|memalign|valloc|free)\(|(^|[^A-Za-z0-9_.])(strdup|strndup|wcsdup|reallocarray)\('
affected="$(find "$LIBC_DIR" -name '*.zig' ! -name malloc.zig -print0 |
  xargs -0 awk -v pat="$ALLOCATING_CALL" '
    match($0, /fn [A-Za-z0-9_]+\(/) { fn = substr($0, RSTART + 3, RLENGTH - 4); next }
    $0 ~ pat && $0 !~ /^[[:space:]]*\/\// { print fn }
  ' | sort -u)"

exported_allocator="$(sed -n 's/.*symbol(&[A-Za-z0-9_]*, "\([A-Za-z0-9_]*\)").*/\1/p' "$LIBC_DIR/malloc.zig" | sort -u)"

rc=0
for f in $affected; do
  if [[ " $OVERRIDDEN " != *" $f "* ]]; then
    echo "check-zig-libc: zig libc function '$f' allocates with zig's allocator but is not overridden in src/zig_libc_compat.c" >&2
    rc=1
  fi
done
for f in $exported_allocator; do
  if [[ " $ALLOCATOR " != *" $f "* ]]; then
    echo "check-zig-libc: zig libc allocator function '$f' is not known to be replaced by jemalloc or src/zig_libc_compat.c" >&2
    rc=1
  fi
done

if [[ $rc -eq 0 ]]; then
  echo "check-zig-libc: OK (zig $("$ZIG" version))"
fi
exit $rc
