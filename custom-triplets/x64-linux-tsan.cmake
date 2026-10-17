# x64-linux with all dependencies instrumented by ThreadSanitizer, so TSan can
# see the synchronization inside folly/proxygen instead of reporting false positives
set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)
set(VCPKG_CMAKE_SYSTEM_NAME Linux)

# hs5 is built as RelWithDebInfo against these, so the debug variants are not needed
set(VCPKG_BUILD_TYPE release)

set(VCPKG_C_FLAGS "-fsanitize=thread -fno-omit-frame-pointer -g")
set(VCPKG_CXX_FLAGS "-fsanitize=thread -fno-omit-frame-pointer -g")
set(VCPKG_LINKER_FLAGS "-fsanitize=thread")
