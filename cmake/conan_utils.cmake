## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

option(runConanAutomatically "Run Conan automatically when configuring the project." ON)

find_program(CONAN_EXECUTABLE NAMES conan NO_CMAKE_SYSTEM_PATH NO_CMAKE_PATH)
if(NOT CONAN_EXECUTABLE)
    message(FATAL_ERROR "Conan executable is not found.")
endif()

set(customConanHome "" CACHE STRING "Custom Conan home directory.")
if(customConanHome)
    message(STATUS "Using custom CONAN_USER_HOME: ${customConanHome}")
    set(ENV{CONAN_USER_HOME} ${customConanHome})
else()
    message(STATUS "Using local CONAN_USER_HOME: ${CMAKE_BINARY_DIR}")
    set(ENV{CONAN_USER_HOME} ${CMAKE_BINARY_DIR})
endif()

if(NOT customConanHome)
    # Sometimes Conan cannot update this file even if it was not modified. A simple solution is
    # just to delete it and let Conan regenerate it.
    file(REMOVE ${CMAKE_BINARY_DIR}/.conan/settings.yml)
endif()

# Install config if it's not installed yet. For non-developer builds config installation is forced
# by default to ensure that CI runs on the most recent config.
option(forceConanConfigInstallation
    "Force Conan to refresh config on each run."
    ${nonDeveloperBuild})
set(config_needs_to_be_installed TRUE)
if(NOT forceConanConfigInstallation)
    nx_execute_process_or_fail(
        COMMAND ${CONAN_EXECUTABLE} config install --list
        OUTPUT_VARIABLE conan_config_list_output
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_MESSAGE "Cannot obtain conan config.")
    message(STATUS "Conan config install list: ${conan_config_list_output}")
    string(REGEX MATCH "${conanConfigUrl}" result "${conan_config_list_output}")
    if(NOT "${result}" STREQUAL "")
        set(config_needs_to_be_installed FALSE)
    endif()
endif()

if(config_needs_to_be_installed)
    message(STATUS "Conan is not configured yet. Running conan config install.")
    nx_execute_process_or_fail(
        COMMAND ${CONAN_EXECUTABLE} config install ${conanConfigUrl}
        ERROR_MESSAGE "Could not apply conan configuration from ${conanConfigUrl}.")
endif()

if(DEFINED "ENV{NX_CONAN_DOWNLOAD_CACHE}")
    message(STATUS "Conan download cache: $ENV{NX_CONAN_DOWNLOAD_CACHE}")
    execute_process(
        COMMAND ${CONAN_EXECUTABLE} config set
            storage.download_cache=$ENV{NX_CONAN_DOWNLOAD_CACHE})
endif()

function(nx_run_conan)
    cmake_parse_arguments(CONAN "" "BUILD_TYPE;PROFILE;HOME_DIR" "FLAGS" ${ARGN})

    set(flags)

    if(CONAN_BUILD_TYPE)
        list(APPEND flags "-s build_type=${CONAN_BUILD_TYPE}")
    endif()

    if(CONAN_PROFILE)
        list(APPEND flags "--profile:build=default" "--profile:host=${CONAN_PROFILE}")
    endif()

    list(APPEND flags ${CONAN_FLAGS})

    list(TRANSFORM flags PREPEND "        " OUTPUT_VARIABLE flags)

    set(run_conan_script_contents
        "#!/usr/bin/env -S cmake -P"
        ""
        "set(ENV{CONAN_USER_HOME} $ENV{CONAN_USER_HOME})"
        "execute_process("
        "    COMMAND"
        "        ${CONAN_EXECUTABLE} install ${CMAKE_SOURCE_DIR}"
        "        --install-folder ${CMAKE_BINARY_DIR}"
        "        --update"
        ${flags}
        "    RESULT_VARIABLE result"
        ")"
        "if(NOT result EQUAL 0)"
        "    message(FATAL_ERROR \"Conan failed to install the dependencies.\")"
        "endif()"
        ""
    )
    string(JOIN "\n" run_conan_script_contents ${run_conan_script_contents})
    set(run_conan_script ${CMAKE_BINARY_DIR}/run_conan.cmake)
    file(CONFIGURE OUTPUT ${run_conan_script} CONTENT ${run_conan_script_contents})
    file(CHMOD ${run_conan_script} PERMISSIONS
        OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
    nx_store_known_file(${run_conan_script})

    if(runConanAutomatically OR NOT EXISTS ${CMAKE_BINARY_DIR}/conan_paths.cmake)
        nx_execute_process_or_fail(
            COMMAND ${CMAKE_COMMAND} -P ${run_conan_script}
            ERROR_MESSAGE "Conan execution failed."
        )
    else()
        message(WARNING
            "Automatic Conan execution is disabled! "
            "If you need to update Conan packages, either enable automatic updates with "
            "`-DrunConanAutomatically=ON` or run update manually using "
            "`cmake -P ${run_conan_script}`")
    endif()
endfunction()
