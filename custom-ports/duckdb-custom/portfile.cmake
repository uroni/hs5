vcpkg_from_github(
        OUT_SOURCE_PATH SOURCE_PATH
        REPO duckdb/duckdb
        REF v${VERSION}
        SHA512 8e725d94cfd81989d4f6d206728188e5b290ce3a7f71d89adc6beed91957f965180d34d69d9099d04e35fc402b389de56184875397b29286789bd9c5655595c5
        HEAD_REF main
    PATCHES
        extensions.patch
)

# Remove vendored dependencies which are not properly namespaced
file(REMOVE_RECURSE
    "${SOURCE_PATH}/third_party/catch"
    "${SOURCE_PATH}/third_party/imdb"
    "${SOURCE_PATH}/third_party/snowball"
    "${SOURCE_PATH}/third_party/tpce-tool"
)

if("excel" IN_LIST FEATURES)
    vcpkg_from_github(
        OUT_SOURCE_PATH DUCKDB_EXCCEL_SOURCE_PATH
        REPO duckdb/duckdb-excel
        REF 0f1df3b14ad6458b90b52c5f625b409a44648c05
        SHA512 9a2ea6e14438aaeda812fe52156401b44293cd25ddede2b79753ca67f1a57e72e54aabb41d811e44444686de1f5b45d367aa5ebb7ded5991a6db8650edf05416
        HEAD_REF main
        PATCHES
            excel-libname.patch
    )
    file(RENAME "${DUCKDB_EXCCEL_SOURCE_PATH}" "${SOURCE_PATH}/extension/excel")
endif()

if("httpfs" IN_LIST FEATURES)
    vcpkg_from_github(
        OUT_SOURCE_PATH DUCKDB_HTTPFS_SOURCE_PATH
        REPO duckdb/duckdb_httpfs
        REF b17b647ea893c3dd9c7e57f66d249aa48e3d4c3f
        SHA512 4c8f70e6cfa482b9ebea955f0c63a1a738f4daa6ebba54171ef17a756b84aa051fd6b888fe86c38d1078ee23e39a3678fedeab35f1da84a80f52192753c5ad3b
        HEAD_REF main
    )
    file(RENAME "${DUCKDB_HTTPFS_SOURCE_PATH}" "${SOURCE_PATH}/extension/httpfs")
endif()

if("ui" IN_LIST FEATURES)
    vcpkg_from_github(
        OUT_SOURCE_PATH DUCKDB_UI_SOURCE_PATH
        REPO duckdb/duckdb-ui
        REF 56d7e670b7e25094d4a216a27495eef06d058cfd
        SHA512 a5e694de59ab284903baadd264117a3a2f7ef4846d8b30a6890d18a49dada95f61e7919f420376ecd158d36ec07476ec2501866f294cd8f576f83d46c31a2fc5
        HEAD_REF main
    )
    file(RENAME "${DUCKDB_UI_SOURCE_PATH}" "${SOURCE_PATH}/extension/ui")
endif()

if("ducklake" IN_LIST FEATURES)
    vcpkg_from_github(
        OUT_SOURCE_PATH DUCKDB_LAKE_SOURCE_PATH
        REPO duckdb/ducklake
        REF 6865d6951569a34bd992be894e08be5af6af0a85
        SHA512 321f54aef0126dccd9fec6e16bf746649bc967630c8a5877c543efd7fc25c3568668e63d50ae02bfdf88ce9546ebae3c661eb94a94ab814a0299ef097272a457
        HEAD_REF main
    )
    file(RENAME "${DUCKDB_LAKE_SOURCE_PATH}" "${SOURCE_PATH}/extension/ducklake")
endif()

string(COMPARE EQUAL "${VCPKG_LIBRARY_LINKAGE}" "static" DUCKDB_BUILD_STATIC)
string(COMPARE EQUAL "${VCPKG_LIBRARY_LINKAGE}" "dynamic" DUCKDB_BUILD_DYNAMIC)

set(EXTENSION_LIST "autocomplete;excel;httpfs;icu;json;tpcds;tpch;ui;ducklake")
set(BUILD_EXTENSIONS "")
foreach(EXT ${EXTENSION_LIST})
    if(${EXT} IN_LIST FEATURES)
        list(APPEND BUILD_EXTENSIONS ${EXT})
    endif()
endforeach()
if(NOT "${BUILD_EXTENSIONS}" STREQUAL "")
    set(BUILD_EXTENSIONS_FLAG "-DBUILD_EXTENSIONS='${BUILD_EXTENSIONS}'")
endif()

vcpkg_cmake_configure(
        SOURCE_PATH ${SOURCE_PATH}
        OPTIONS
            -DOVERRIDE_GIT_DESCRIBE=v${VERSION}
            -DDUCKDB_EXPLICIT_VERSION=v${VERSION}
            -DBUILD_UNITTESTS=OFF
            -DBUILD_SHELL=FALSE
            "${BUILD_EXTENSIONS_FLAG}"
            -DENABLE_EXTENSION_AUTOLOADING=1
            -DENABLE_EXTENSION_AUTOINSTALL=1
            -DWITH_INTERNAL_ICU=OFF
            -DENABLE_SANITIZER=OFF
            -DENABLE_THREAD_SANITIZER=OFF
            -DENABLE_UBSAN=OFF
)

vcpkg_cmake_install()

if(EXISTS "${CURRENT_PACKAGES_DIR}/CMake")
    vcpkg_cmake_config_fixup(CONFIG_PATH CMake)
elseif(EXISTS "${CURRENT_PACKAGES_DIR}/lib/cmake/DuckDB")
    vcpkg_cmake_config_fixup(CONFIG_PATH "lib/cmake/DuckDB")
elseif(EXISTS "${CURRENT_PACKAGES_DIR}/lib/cmake/${PORT}")
    vcpkg_cmake_config_fixup(CONFIG_PATH "lib/cmake/${PORT}")
endif()

if(VCPKG_LIBRARY_LINKAGE STREQUAL static)
    file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/bin" "${CURRENT_PACKAGES_DIR}/debug/bin")
endif()

file(REMOVE_RECURSE
    "${CURRENT_PACKAGES_DIR}/include/duckdb/main/capi/header_generation"
)

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/share")
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/include/duckdb/storage/serialization")
file(INSTALL "${CMAKE_CURRENT_LIST_DIR}/usage" DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}")
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")
