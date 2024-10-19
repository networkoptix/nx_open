## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

set(depends)

function(_prepare_cmake_flags result_variable)
    set(result)
    foreach(var ${ARGN})
        if(NOT "${${var}}" STREQUAL "")
            list(APPEND result -D${var}=${${var}})
        endif()
    endforeach()
    set(${result_variable} ${result} PARENT_SCOPE)
endfunction()

macro(build_client_for_abi abi)
    _prepare_cmake_flags(_cmake_args
        buildNumber
        customization
        cloudGroup
        customCloudHost
        developerBuild
        publicationType
        useProdNotificationSettings
        pushNotificationProvider
        enablePrecompiledHeaders
    )

    # If CMake configuration of the root project was cleared, we need to clear the child
    # configurations as well.
    if(NOT cmakeWasConfiguredBefore)
        file(REMOVE
            ${CMAKE_CURRENT_BINARY_DIR}/android_${abi}/src/android_${abi}-build/CMakeCache.txt
            ${CMAKE_CURRENT_BINARY_DIR}/android_${abi}/src/android_${abi}-stamp/android_${abi}-configure)
    endif()

    ExternalProject_Add(android_${abi}
        SOURCE_DIR ${CMAKE_SOURCE_DIR}
        PREFIX android_${abi}
        INSTALL_COMMAND ${CMAKE_COMMAND} -E echo "Skipping install step..."
        CMAKE_ARGS
            -DtargetDevice=android_${abi}
            ${_cmake_args}
        BUILD_ALWAYS TRUE
        USES_TERMINAL_CONFIGURE TRUE
        USES_TERMINAL_BUILD TRUE
    )

    unset(_cmake_args)
    list(APPEND android_abis ${abi})
    list(APPEND depends android_${abi})
endmacro()

set(android_abis)

build_client_for_abi(arm32)
build_client_for_abi(arm64)

string(REPLACE ";" " " android_abis "${android_abis}")
nx_configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/build_distribution.conf.in
    ${CMAKE_CURRENT_BINARY_DIR}/build_distribution.conf
    @ONLY)
list(APPEND configured_files ${CMAKE_CURRENT_BINARY_DIR}/build_distribution.conf)

# Tracking dependencies of AAB which uses two external projects is not a simple task. It's ok to
# re-build it every time.
set(script ${CMAKE_CURRENT_SOURCE_DIR}/build_distribution.sh)
if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin")
    # Bash in macOS is no longer updated and does not support some features. However Zsh works well
    # in our case.
    set(shell /bin/zsh)
else()
    set(shell /bin/bash)
endif()
add_custom_target(distribution_aab_mobile_client ALL
    COMMAND ${shell} ${script}
    DEPENDS ${script} ${depends} ${configured_files}
    COMMENT "Creating Android application bundle for mobile client"
)
