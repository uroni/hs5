cmake_minimum_required(VERSION 3.21)
project(hs5 VERSION 0.1.7 LANGUAGES CXX C)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_custom_target(sqlgen)
add_custom_command(
  TARGET   sqlgen
  COMMAND  "${_VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin/sqlite-cpp-sqlgen" "template.db" "DbDao.cpp"
  WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}"
)

set(APIGEN_SCHEMAS
  AddUserParams
  LoginParams
  LoginResp
  HapiError
  AddUserResp
  ListParams
  ListResp
  EmptyParams
  AddBucketParams
  EmptyResp # Must be last
)  


add_custom_target(apigen)
foreach(SCHEMA ${APIGEN_SCHEMAS})
  add_custom_command(
    TARGET   apigen
    COMMAND  "echo" "Generating ${SCHEMA}..."
    WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}"
  )
  add_custom_command(
    TARGET   apigen
    COMMAND  "quicktype" "-s" "schema" "src/schemas/${SCHEMA}.json" "-o" "src/apigen/${SCHEMA}.cpp" "--include-location" "global-include" "--type-style" "pascal-case" "--member-style" "camel-case"  "--enumerator-style" "camel-case" "--no-boost" "--source-style" "multi-source" "--namespace" "Api" "--code-format" "with-struct"
    WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}"
  )
  add_custom_command(TARGET apigen
                   COMMAND ${CMAKE_COMMAND} -E rename
                   "src/apigen/Generators.hpp"
                   "src/apigen/Generators${SCHEMA}.hpp"
                   WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}")
endforeach()

set(SCHEMA_SOURCES "")
foreach(SCHEMA ${APIGEN_SCHEMAS})
  if(EXISTS "src/apigen/${SCHEMA}.cpp")
    list(APPEND SCHEMA_SOURCES "src/apigen/${SCHEMA}.cpp")
  endif()
endforeach()

# Get the current git revision
execute_process(
  COMMAND git rev-parse --short HEAD
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  OUTPUT_VARIABLE GIT_REVISION
  OUTPUT_STRIP_TRAILING_WHITESPACE
)

# Configure config.h
configure_file(
  ${CMAKE_SOURCE_DIR}/src/config.h.in
  ${CMAKE_BINARY_DIR}/config.h
  @ONLY
)

add_executable(hs5
    external/lmdb/midl.cpp
    external/lmdb/mdb.c
    external/lmdb/cppmidl.cpp
    src/main.cpp
    src/s3handler.cpp
    src/SingleFileStorage.cpp
    src/data.cpp
    src/os_functions.cpp 
    src/utils.cpp
    src/crypt.cpp
    src/File.cpp
    src/Buckets.cpp
    src/DbDao.cpp
    src/Policy.cpp
    src/Auth.cpp
    src/ApiHandler.cpp
    src/Session.cpp
    src/cmd.cpp
    src/StaticHandler.cpp
    wwwgen/www_files.cpp
    ${SCHEMA_SOURCES})

set_property(TARGET hs5 PROPERTY CXX_STANDARD 20)

# Include the generated config.h
target_include_directories(hs5 PRIVATE ${CMAKE_BINARY_DIR})

#set(GCC_COVERAGE_COMPILE_FLAGS "-fcoroutines")

add_definitions(${GCC_COVERAGE_COMPILE_FLAGS})

find_package(fmt CONFIG REQUIRED)
find_package(folly CONFIG REQUIRED)
find_package(proxygen CONFIG REQUIRED)
find_package(zstd CONFIG REQUIRED)
find_package(expat CONFIG REQUIRED)
find_package(unofficial-sodium CONFIG REQUIRED)
find_package(sqlite-cpp-sqlgen CONFIG REQUIRED)
find_package(nlohmann_json  CONFIG REQUIRED)
find_path(TCLAP_INCLUDE_DIRS "tclap/Arg.h")
target_include_directories(hs5 PRIVATE ${TCLAP_INCLUDE_DIRS})
target_link_libraries(hs5 PRIVATE $<IF:$<TARGET_EXISTS:zstd::libzstd_shared>,zstd::libzstd_shared,zstd::libzstd_static> Folly::folly Folly::folly_deps Folly::follybenchmark Folly::folly_test_util
  proxygen::proxygen proxygen::proxygencurl proxygen::proxygenhttpserver expat::expat unofficial-sodium::sodium SqliteCppGen::SqliteCppGen nlohmann_json::nlohmann_json fmt::fmt)


if(CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_CONFIGURATION_TYPES MATCHES ".*[dD]ebug.*")
  add_compile_options(-fsanitize=address)
  add_link_options(-fsanitize=address)
endif()

target_compile_features(hs5 PUBLIC cxx_std_20)

