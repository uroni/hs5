cmake_minimum_required(VERSION 3.21...4.0)

if(DEFINED CMAKE_TOOLCHAIN_FILE AND NOT EXISTS "${CMAKE_TOOLCHAIN_FILE}")
  string(REGEX REPLACE
    "[/\\\\]scripts[/\\\\]buildsystems[/\\\\]vcpkg\\.cmake$"
    ""
    HS5_VCPKG_ROOT
    "${CMAKE_TOOLCHAIN_FILE}"
  )

  if(NOT HS5_VCPKG_ROOT STREQUAL "${CMAKE_TOOLCHAIN_FILE}")
    set(HS5_VCPKG_GIT_COMMIT "d7b75fa8a30478ae5b5eb86ac730f851ef036875")
    find_program(GIT_EXECUTABLE git REQUIRED)

    if(EXISTS "${HS5_VCPKG_ROOT}" AND NOT EXISTS "${HS5_VCPKG_ROOT}/.git")
      message(FATAL_ERROR
        "Expected vcpkg checkout at ${HS5_VCPKG_ROOT}, but the directory exists "
        "and is not a git repository."
      )
    endif()

    if(NOT EXISTS "${HS5_VCPKG_ROOT}")
      message(STATUS "Cloning vcpkg into ${HS5_VCPKG_ROOT}")
      execute_process(
        COMMAND "${GIT_EXECUTABLE}" clone https://github.com/microsoft/vcpkg "${HS5_VCPKG_ROOT}"
        RESULT_VARIABLE HS5_VCPKG_CLONE_RESULT
      )

      if(NOT HS5_VCPKG_CLONE_RESULT EQUAL 0)
        message(FATAL_ERROR "Failed to clone vcpkg into ${HS5_VCPKG_ROOT}")
      endif()
    endif()

    message(STATUS "Checking out vcpkg commit ${HS5_VCPKG_GIT_COMMIT}")
    execute_process(
      COMMAND "${GIT_EXECUTABLE}" checkout "${HS5_VCPKG_GIT_COMMIT}"
      WORKING_DIRECTORY "${HS5_VCPKG_ROOT}"
      RESULT_VARIABLE HS5_VCPKG_CHECKOUT_RESULT
    )

    if(NOT HS5_VCPKG_CHECKOUT_RESULT EQUAL 0)
      message(FATAL_ERROR "Failed to checkout vcpkg commit ${HS5_VCPKG_GIT_COMMIT}")
    endif()
  endif()
endif()

project(hs5 VERSION 1.1.0 LANGUAGES CXX C)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_custom_target(sqlgen
  COMMAND "${_VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin/sqlite-cpp-sqlgen" "template.db" "DbDao.cpp"
  WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/src"
)

set(APIGEN_SCHEMAS
  HapiError
  AddUserParams
  AddUserResp
  RemoveUserParams
  RemoveUserResp
  ListUsersParams
  ListUsersResp
  AddAccessKeyParams
  AddAccessKeyResp
  ListAccessKeysParams
  ListAccessKeysResp
  RemoveAccessKeyParams
  RemoveAccessKeyResp
  AddPolicyParams
  AddPolicyResp
  RemovePolicyParams
  RemovePolicyResp
  ListPoliciesParams
  ListPoliciesResp
  AddRoleParams
  AddRoleResp
  RemoveRoleParams
  RemoveRoleResp
  ListRolesParams
  ListRolesResp
  AddUserRoleParams
  AddUserRoleResp
  RemoveUserRoleParams
  RemoveUserRoleResp
  ListUserRolesParams
  ListUserRolesResp
  AddRolePolicyParams
  AddRolePolicyResp
  RemoveRolePolicyParams
  RemoveRolePolicyResp
  ListRolePoliciesParams
  ListRolePoliciesResp
  ChangePolicyParams
  ChangePolicyResp
  DeleteBucketParams
  DeleteBucketResp
  ChangePasswordParams
  ChangePasswordResp
  LoginParams
  LoginResp    
  ListParams
  ListResp
  AddBucketParams
  AddBucketResp
  LogoutParams
  LogoutResp
  AddBucketPermissionParams
  AddBucketPermissionResp
  ListBucketPermissionsParams
  ListBucketPermissionsResp
  RemoveBucketPermissionParams
  RemoveBucketPermissionResp
  SetBucketPublicParams
  SetBucketPublicResp
  StatsParams
  StatsResp#
  SessionCheckParams
  SessionCheckResp # Must be last
)


add_custom_target(apigen)
foreach(SCHEMA ${APIGEN_SCHEMAS})
  add_custom_command(
    TARGET   apigen
    POST_BUILD
    COMMAND  "echo" "Generating ${SCHEMA}..."
    WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}"
  )
  add_custom_command(
    TARGET   apigen
    POST_BUILD 
    COMMAND  "${CMAKE_CURRENT_LIST_DIR}/quicktype-bundled.js" "-s" "schema" "src/schemas/${SCHEMA}.json" "-o" "src/apigen/${SCHEMA}.cpp" "--include-location" "global-include" "--type-style" "pascal-case" "--member-style" "camel-case"  "--enumerator-style" "camel-case" "--no-boost" "--source-style" "multi-source" "--namespace" "Api" "--code-format" "with-struct"
    WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}"
  )
  add_custom_command(TARGET apigen
                   POST_BUILD
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

# add_compile_options(-fno-omit-frame-pointer)
# if(CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_CONFIGURATION_TYPES MATCHES ".*[dD]ebug.*")
#   add_compile_options(-fsanitize=address)
#   add_link_options(-fsanitize=address)
# endif()

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
    src/WalFile.cpp
    wwwgen/www_files.cpp
    src/DuckDbFs.cpp
    src/Action.cpp
    src/ContentType.cpp
    src/PayloadHash.cpp
    src/UserMetadata.cpp
    src/ObjMetadata.cpp
    external/proxygen/RFC1867.cpp
    ${SCHEMA_SOURCES}
    )

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
find_package(DuckDB CONFIG REQUIRED)
find_package(aws-c-common CONFIG REQUIRED)
find_package(aws-checksums CONFIG REQUIRED)
find_library(JEMALLOC_LIBRARY NAMES jemalloc REQUIRED)
find_path(TCLAP_INCLUDE_DIRS "tclap/Arg.h")
target_include_directories(hs5 PRIVATE ${TCLAP_INCLUDE_DIRS})
target_link_directories(hs5 PRIVATE ${_VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/lib)
# Patch missing transitive dependency: wangle_acceptor uses folly::AsyncFdSocket
# but doesn't declare the dependency in its CMake config
if(TARGET wangle::wangle_acceptor)
  set_property(TARGET wangle::wangle_acceptor APPEND PROPERTY INTERFACE_LINK_LIBRARIES Folly::folly_io_async_fdsock_async_fd_socket)
endif()

target_link_libraries(hs5 PRIVATE $<IF:$<TARGET_EXISTS:zstd::libzstd_shared>,zstd::libzstd_shared,zstd::libzstd_static>
  Folly::folly Folly::folly_deps Folly::follybenchmark Folly::folly_test_util
  proxygen::proxygen proxygen::proxygenhttpserver
  expat::expat unofficial-sodium::sodium SqliteCppGen::SqliteCppGen nlohmann_json::nlohmann_json fmt::fmt
  $<IF:$<TARGET_EXISTS:duckdb_static>,duckdb_static,duckdb> AWS::aws-c-common AWS::aws-checksums ${JEMALLOC_LIBRARY})

target_compile_features(hs5 PUBLIC cxx_std_20)
target_compile_definitions(hs5 PRIVATE USE_JEMALLOC)
