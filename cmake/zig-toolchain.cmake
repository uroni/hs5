# Builds with zig against musl, producing fully static binaries.
# Used both for hs5 itself and (via the *-linux-zig-static triplets) for the vcpkg dependencies.
# The compiler wrappers in cmake/zig select the musl target for the host architecture.
set(CMAKE_SYSTEM_NAME Linux)

set(CMAKE_C_COMPILER "${CMAKE_CURRENT_LIST_DIR}/zig/zig-cc")
set(CMAKE_CXX_COMPILER "${CMAKE_CURRENT_LIST_DIR}/zig/zig-c++")
set(CMAKE_AR "${CMAKE_CURRENT_LIST_DIR}/zig/zig-ar" CACHE FILEPATH "")
set(CMAKE_RANLIB "${CMAKE_CURRENT_LIST_DIR}/zig/zig-ranlib" CACHE FILEPATH "")

set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_EXE_LINKER_FLAGS_INIT "-static")
