cmake_minimum_required(VERSION 3.21...4.0)

include("${CMAKE_CURRENT_LIST_DIR}/cmake/BootstrapVcpkg.cmake")

# Has to be decided before project(), where the vcpkg toolchain installs the manifest dependencies
option(HS5_WITH_DUCKDB "Build with DuckDB UI support" ON)
if(NOT HS5_WITH_DUCKDB)
  set(VCPKG_MANIFEST_NO_DEFAULT_FEATURES ON)
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


find_program(CLANG_FORMAT_EXECUTABLE clang-format HINTS "${CMAKE_CURRENT_LIST_DIR}/.venv/bin")
find_program(BLACK_EXECUTABLE black HINTS "${CMAKE_CURRENT_LIST_DIR}/.venv/bin")
if(CLANG_FORMAT_EXECUTABLE AND BLACK_EXECUTABLE)
  set(HS5_FORMAT_CPP_FILES "git ls-files -z 'src/*.cpp' 'src/*.h' ':!src/apigen/*' | xargs -0 \"${CLANG_FORMAT_EXECUTABLE}\"")
  add_custom_target(format
    COMMAND sh -c "${HS5_FORMAT_CPP_FILES} -i"
    COMMAND "${BLACK_EXECUTABLE}" .
    WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}"
    VERBATIM
  )
  add_custom_target(format-check
    COMMAND sh -c "${HS5_FORMAT_CPP_FILES} --dry-run --Werror"
    COMMAND "${BLACK_EXECUTABLE}" --check --diff .
    WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}"
    VERBATIM
  )
endif()

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

# LMDB accesses DUPSORT sub-pages stored inline in nodes (only 2-byte aligned) as MDB_page
if(CMAKE_C_FLAGS MATCHES "-fsanitize=[^ ]*undefined")
  set_source_files_properties(external/lmdb/mdb.c PROPERTIES COMPILE_OPTIONS "-fno-sanitize=alignment")
endif()

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
    src/ObjectReader.cpp
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
find_package(OpenSSL REQUIRED)
find_package(sqlite-cpp-sqlgen CONFIG REQUIRED)
find_package(nlohmann_json  CONFIG REQUIRED)
if(HS5_WITH_DUCKDB)
  find_package(DuckDB CONFIG REQUIRED)
  target_sources(hs5 PRIVATE src/DuckDbFs.cpp)
  target_compile_definitions(hs5 PRIVATE HS5_WITH_DUCKDB)
  target_link_libraries(hs5 PRIVATE $<IF:$<TARGET_EXISTS:duckdb_static>,duckdb_static,duckdb>)
endif()
find_package(aws-c-common CONFIG REQUIRED)
find_package(aws-checksums CONFIG REQUIRED)
option(HS5_USE_JEMALLOC "Link jemalloc as the memory allocator" ON)
if(HS5_USE_JEMALLOC)
  find_library(JEMALLOC_LIBRARY NAMES jemalloc REQUIRED)
endif()
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
  expat::expat unofficial-sodium::sodium OpenSSL::Crypto SqliteCppGen::SqliteCppGen nlohmann_json::nlohmann_json fmt::fmt
  AWS::aws-c-common AWS::aws-checksums $<$<BOOL:${HS5_USE_JEMALLOC}>:${JEMALLOC_LIBRARY}>)

target_compile_features(hs5 PUBLIC cxx_std_20)
if(HS5_USE_JEMALLOC)
  target_compile_definitions(hs5 PRIVATE USE_JEMALLOC)
endif()
# Always use mvfst's expected-lite implementation (nonstd::expected), never std::expected
target_compile_definitions(hs5 PRIVATE nsel_CONFIG_SELECT_EXPECTED=nsel_EXPECTED_NONSTD)
