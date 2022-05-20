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

# Get dependencies.
set(conan_profile ${targetDevice})
if(targetDevice STREQUAL edge1)
    set(conan_profile linux_arm32)
endif()

set(use_clang ${useClang})
nx_to_python_compatible_bool(use_clang)

if(targetDevice MATCHES "windows" AND CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(build_type "Debug")
else()
    set(build_type "Release")
endif()

set(_run_conan_script ${CMAKE_BINARY_DIR}/run_conan.cmake)
set(conan_home $ENV{CONAN_USER_HOME})

set(_additional_conan_parameters)
if(targetDevice MATCHES "^linux|^edge")
    set(_additional_conan_parameters
        "        --profile:host ${open_source_root}/cmake/conan_profiles/gcc.profile"
    )
endif()

set(_run_conan_script_contents
    "#!/usr/bin/env -S cmake -P"
    ""
    "set(ENV{CONAN_USER_HOME} ${conan_home})"
    "execute_process("
    "    COMMAND"
    "        ${CONAN_EXECUTABLE} install ${CMAKE_SOURCE_DIR}"
    "        --install-folder ${CMAKE_BINARY_DIR}"
    "        --profile:build default"
    "        --profile:host ${CMAKE_SOURCE_DIR}/conan_profiles/${conan_profile}.profile"
    "        --update"
    "        -s build_type=${build_type}"
    "        -o targetDevice=${targetDevice}"
    "        -o useClang=${use_clang}"
    ${_additional_conan_parameters}
    "    RESULT_VARIABLE result"
    ")"
    "if(NOT result EQUAL 0)"
    "    message(FATAL_ERROR \"Conan failed to install the dependencies.\")"
    "endif()"
    ""
)
string(JOIN "\n" _run_conan_script_contents ${_run_conan_script_contents})
file(CONFIGURE OUTPUT ${_run_conan_script} CONTENT ${_run_conan_script_contents})
file(CHMOD ${_run_conan_script} PERMISSIONS
    OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
nx_store_known_file(${_run_conan_script})

if(runConanAutomatically OR NOT EXISTS ${CMAKE_BINARY_DIR}/conan_paths.cmake)
    nx_execute_process_or_fail(
        COMMAND ${CMAKE_COMMAND} -P ${_run_conan_script}
        ERROR_MESSAGE "Conan execution failed."
    )
else()
    message(WARNING
        "Automatic Conan execution is disabled! "
        "If you need to update Conan packages, either enable automatic updates with "
        "`-DrunConanAutomatically=ON` or run update manually using "
        "`cmake -P ${_run_conan_script}`")
endif()

set(CONAN_DISABLE_CHECK_COMPILER ON)

include(${CMAKE_BINARY_DIR}/conan_paths.cmake)

nx_check_package_paths_changes(conan ${CONAN_DEPENDENCIES})

# Load dependencies.
if(CONAN_GCC-TOOLCHAIN_ROOT)
    set(CONAN_COMPILER gcc)
    set(ENV{TOOLCHAIN_DIR} ${CONAN_GCC-TOOLCHAIN_ROOT})
endif()

if(CONAN_CLANG_ROOT)
    set(ENV{CLANG_DIR} ${CONAN_CLANG_ROOT})
endif()

if(CONAN_ANDROIDNDK_ROOT)
    set(ENV{ANDROID_NDK} ${CONAN_ANDROIDNDK_ROOT})
endif()

if(CONAN_ANDROIDSDK_ROOT)
    set(ENV{ANDROID_HOME} ${CONAN_ANDROIDSDK_ROOT})
endif()

if(CONAN_QT_ROOT)
    set(QT_DIR ${CONAN_QT_ROOT})
endif()

if(CONAN_MSVC-REDIST_ROOT)
    set(vcrt_directory ${CONAN_MSVC-REDIST_ROOT})
endif()

if(CONAN_DIRECTX_ROOT)
    include(${CONAN_DIRECTX_ROOT}/directx.cmake)
endif()

if(CONAN_WINSDK-REDIST_ROOT)
    set(ucrt_directory ${CONAN_WINSDK-REDIST_ROOT})
endif()

if(CONAN_VMS_HELP_ROOT)
    set(help_directory ${CONAN_VMS_HELP_ROOT})
endif()

if(CONAN_VMS_QUICK_START_GUIDE_ROOT)
    set(quick_start_guide_directory ${CONAN_VMS_QUICK_START_GUIDE_ROOT})
endif()

# Exclude conan-generated files from ninja_clean
foreach(extension "bat" "sh" "ps1")
    nx_store_known_file(${CMAKE_BINARY_DIR}/activate_run.${extension})
    nx_store_known_file(${CMAKE_BINARY_DIR}/deactivate_run.${extension})
    nx_store_known_file(${CMAKE_BINARY_DIR}/environment_run.${extension}.env)
endforeach(extension)

nx_store_known_file(${CMAKE_BINARY_DIR}/lib/ffmpeg)
