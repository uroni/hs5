cmake_minimum_required(VERSION 3.21)
project(hs5 VERSION 0.1.0 LANGUAGES CXX C)

add_executable(hs5
    lmdb/midl.cpp
    lmdb/mdb.c
    lmdb/cppmidl.cpp
    main.cpp
    s3handler.cpp
    SingleFileStorage.cpp
    data.cpp
    os_functions.cpp 
    utils.cpp
    crypt.cpp )

#set(GCC_COVERAGE_COMPILE_FLAGS "-fcoroutines")

add_definitions(${GCC_COVERAGE_COMPILE_FLAGS})

find_package(folly CONFIG REQUIRED)
find_package(proxygen CONFIG REQUIRED)
find_package(zstd CONFIG REQUIRED)
find_package(expat CONFIG REQUIRED)
find_package(unofficial-sodium CONFIG REQUIRED)
target_link_libraries(hs5 PRIVATE $<IF:$<TARGET_EXISTS:zstd::libzstd_shared>,zstd::libzstd_shared,zstd::libzstd_static> Folly::folly Folly::folly_deps Folly::follybenchmark Folly::folly_test_util proxygen::proxygen proxygen::proxygencurl proxygen::proxygenhttpserver expat::expat unofficial-sodium::sodium unofficial-sodium::sodium_config_public)


target_compile_features(hs5 PUBLIC cxx_std_20)