# Included after the portfile of selected ports by the *-linux-zig-static triplets

# folly's exception tracer defines __cxa_throw etc. to wrap the C++ runtime's versions via
# dlsym(RTLD_NEXT), which only works with a shared C++ runtime. Linked fully statically it
# clashes with libc++abi's definitions, so remove it (hs5 does not use it).
if(PORT STREQUAL "folly")
    foreach(lib IN ITEMS "${CURRENT_PACKAGES_DIR}/lib/libfolly.a" "${CURRENT_PACKAGES_DIR}/debug/lib/libfolly.a")
        if(EXISTS "${lib}")
            vcpkg_execute_required_process(
                COMMAND "${CMAKE_CURRENT_LIST_DIR}/zig/zig-ar" d "${lib}" ExceptionTracerLib.cpp.o
                WORKING_DIRECTORY "${CURRENT_BUILDTREES_DIR}"
                LOGNAME "remove-exception-tracer-${TARGET_TRIPLET}"
            )
        endif()
    endforeach()
endif()
