cmake_minimum_required(VERSION 3.21...4.0)
project(hs5 VERSION 0.18.1 LANGUAGES CXX C)

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
find_path(TCLAP_INCLUDE_DIRS "tclap/Arg.h")
target_include_directories(hs5 PRIVATE ${TCLAP_INCLUDE_DIRS})
target_link_libraries(hs5 PRIVATE $<IF:$<TARGET_EXISTS:zstd::libzstd_shared>,zstd::libzstd_shared,zstd::libzstd_static> Folly::folly Folly::folly_deps Folly::follybenchmark Folly::folly_test_util
  proxygen::proxygen proxygen::proxygencurl proxygen::proxygenhttpserver expat::expat unofficial-sodium::sodium SqliteCppGen::SqliteCppGen nlohmann_json::nlohmann_json fmt::fmt
  $<IF:$<TARGET_EXISTS:duckdb_static>,duckdb_static,duckdb> AWS::aws-c-common AWS::aws-checksums)

target_compile_features(hs5 PUBLIC cxx_std_20)

