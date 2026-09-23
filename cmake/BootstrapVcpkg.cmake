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
