# Builds with zig against musl, producing fully static binaries.
# Used both for hs5 itself and (via the *-linux-zig-static triplets) for the vcpkg dependencies.
# The compiler wrappers in cmake/zig select the musl target for the host architecture.
set(CMAKE_SYSTEM_NAME Linux)
# Setting CMAKE_SYSTEM_NAME marks this as cross compiling, which leaves CMAKE_SYSTEM_PROCESSOR
# empty (breaks e.g. folly's arch checks). The wrappers target the host architecture.
set(CMAKE_SYSTEM_PROCESSOR "${CMAKE_HOST_SYSTEM_PROCESSOR}")

set(CMAKE_C_COMPILER "${CMAKE_CURRENT_LIST_DIR}/zig/zig-cc")
set(CMAKE_CXX_COMPILER "${CMAKE_CURRENT_LIST_DIR}/zig/zig-c++")
set(CMAKE_AR "${CMAKE_CURRENT_LIST_DIR}/zig/zig-ar" CACHE FILEPATH "")
set(CMAKE_RANLIB "${CMAKE_CURRENT_LIST_DIR}/zig/zig-ranlib" CACHE FILEPATH "")

# zig does not ship clang-scan-deps, which CMake uses to scan C++20 sources for modules
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_EXE_LINKER_FLAGS_INIT "-static")

# As chainloaded toolchain this replaces vcpkg's linux toolchain, so apply the triplet flags here
string(APPEND CMAKE_C_FLAGS_INIT " ${VCPKG_C_FLAGS}")
string(APPEND CMAKE_CXX_FLAGS_INIT " ${VCPKG_CXX_FLAGS}")
string(APPEND CMAKE_ASM_FLAGS_INIT " ${VCPKG_C_FLAGS}")
