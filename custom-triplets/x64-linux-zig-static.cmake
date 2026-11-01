# x64-linux with all dependencies built statically by zig against musl,
# so hs5 can be linked into a fully static binary
set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE static)
set(VCPKG_LIBRARY_LINKAGE static)
set(VCPKG_CMAKE_SYSTEM_NAME Linux)

# hs5 is built as Release against these, so the debug variants are not needed
set(VCPKG_BUILD_TYPE release)

# musl does not define __ELF_NATIVE_CLASS (glibc does), which folly's symbolizer needs,
# and only provides off64_t etc. (used by folly) with _LARGEFILE64_SOURCE
set(VCPKG_C_FLAGS "-D__ELF_NATIVE_CLASS=64 -D_LARGEFILE64_SOURCE")
set(VCPKG_CXX_FLAGS "-D__ELF_NATIVE_CLASS=64 -D_LARGEFILE64_SOURCE")

# librt is part of musl's libc, so there is no separate library for find_library to find
# (fizz, wangle and mvfst require it); zig accepts -lrt
set(VCPKG_CMAKE_CONFIGURE_OPTIONS "-DLIBRT_LIBRARY:STRING=rt")

set(VCPKG_CHAINLOAD_TOOLCHAIN_FILE "${CMAKE_CURRENT_LIST_DIR}/../cmake/zig-toolchain.cmake")
