## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

include_guard(GLOBAL)

include(${CMAKE_CURRENT_LIST_DIR}/output_directories.cmake)

option(runConanAutomatically "Run Conan automatically when configuring the project." ON)
set(extraConanArgs "" CACHE STRING "Extra command line arguments for Conan.")
set(customConanHome "" CACHE STRING "Custom Conan home directory.")

string(CONCAT installSystemRequirementsDoc
    "Allow installation of system requirements by Conan. "
    "This option does only work in an interactive shell.")
option(installSystemRequirements ${installSystemRequirementsDoc} OFF)
# Due to security reasons `installSystemRequirements` should be reset to OFF after every CMake run.
cmake_language(DEFER
    DIRECTORY ${CMAKE_SOURCE_DIR}
    CALL set installSystemRequirements OFF CACHE BOOL ${installSystemRequirementsDoc} FORCE
)

function(nx_init_conan)
    find_program(CONAN_EXECUTABLE NAMES conan NO_CMAKE_SYSTEM_PATH NO_CMAKE_PATH)
    if(NOT CONAN_EXECUTABLE)
        message(FATAL_ERROR "Conan executable is not found.")
    endif()

    if(customConanHome)
        message(STATUS "Using custom CONAN_HOME: ${customConanHome}")
        set(ENV{CONAN_HOME} ${customConanHome})
    else()
        message(STATUS "Using local CONAN_HOME: ${CMAKE_BINARY_DIR}/.conan2")
        set(ENV{CONAN_HOME} ${CMAKE_BINARY_DIR}/.conan2)
    endif()

    _configure_conan()
endfunction()

function(_configure_conan)
    if(NOT customConanHome)
        set(conan_installed_config_list_file "$ENV{CONAN_HOME}/config_install.json")
        file(REMOVE ${conan_installed_config_list_file})

        set(conan_config_dir_src "${open_source_root}/conan_config")
        set(conan_config_dir "${CMAKE_BINARY_DIR}/conan_config")
        nx_configure_directory(${conan_config_dir_src} ${conan_config_dir})

        nx_execute_process_or_fail(
            COMMAND ${CONAN_EXECUTABLE} config install ${conan_config_dir}
            ERROR_MESSAGE "Can not apply Conan configuration from ${conan_config_dir}"
        )

        # Create default profile if it doesn't exist
        if(NOT EXISTS "$ENV{CONAN_HOME}/profiles/default")
            nx_execute_process_or_fail(
                COMMAND ${CONAN_EXECUTABLE} profile detect
                ERROR_MESSAGE "Can not detect default Conan profile"
            )
        endif()
    else()
        # Check if "nx" remote is already added.
        set(conan_remotes_file "$ENV{CONAN_HOME}/remotes.json")
        if(EXISTS ${conan_remotes_file})
            file(READ ${conan_remotes_file} conan_remotes)
            string(JSON conan_remotes_count LENGTH ${conan_remotes} "remotes")
            math(EXPR last_conan_remote_index "${conan_remotes_count} - 1")
            foreach(remote_index RANGE 0 ${last_conan_remote_index})
                string(JSON remote_url GET ${conan_remotes} "remotes" ${remote_index} "url")
                string(JSON remote_name GET ${conan_remotes} "remotes" ${remote_index} "name")
                if("${remote_name}" STREQUAL "nx" AND "${remote_url}" STREQUAL "${conanNxRemote}")
                    return()
                endif()
            endforeach()
        endif()

        nx_execute_process_or_fail(
            COMMAND ${CONAN_EXECUTABLE} remote add nx ${conanNxRemote} --force
            ERROR_MESSAGE "Can not add Nx Conan remote."
        )
    endif()

    if(DEFINED ENV{NX_ARTIFACTORY_USERNAME} AND DEFINED ENV{NX_ARTIFACTORY_PASSWORD})
        message(STATUS "Using custom conan credentials $ENV{NX_ARTIFACTORY_USERNAME}:******")
        nx_execute_process_or_fail(
            COMMAND ${CONAN_EXECUTABLE} remote login nx
                $ENV{NX_ARTIFACTORY_USERNAME} -p $ENV{NX_ARTIFACTORY_PASSWORD}
            ERROR_MESSAGE "Can not authenticate with Conan remote."
        )
    endif()
endfunction()

function(nx_run_conan)
    cmake_parse_arguments(CONAN "" "BUILD_TYPE;PROFILE;HOME_DIR" "FLAGS" ${ARGN})

    set(flags)

    if(CONAN_BUILD_TYPE)
        list(APPEND flags "-s build_type=${CONAN_BUILD_TYPE}")
    endif()

    if(CONAN_PROFILE)
        list(APPEND flags "--profile:host=${CONAN_PROFILE}")
    endif()

    list(APPEND flags ${CONAN_FLAGS})

    if(DEFINED ENV{NX_CONAN_DOWNLOAD_CACHE})
        cmake_path(CONVERT "$ENV{NX_CONAN_DOWNLOAD_CACHE}" TO_CMAKE_PATH_LIST conanDownloadCache)
        message(STATUS "Conan download cache: ${conanDownloadCache}")
        list(APPEND flags
            "--core-conf core.download:download_cache=${conanDownloadCache}")
    endif()

    if(WIN32)
        # Generate PowerShell runtime activation scripts (conanrun.ps1) alongside the default
        # .bat/.sh. Conan2 does not emit these by default, leaving IDEs such as CLion without a
        # way to set up the runtime environment (e.g. Qt libs) before launching a target.
        list(APPEND flags "-c tools.env.virtualenv:powershell=powershell.exe")
    endif()

    list(APPEND flags ${extraConanArgs})

    set(CONAN_HOME $ENV{CONAN_HOME})

    set(run_conan_script ${CMAKE_BINARY_DIR}/run_conan.cmake)
    nx_configure_file(${open_source_root}/cmake/run_conan.cmake.in ${run_conan_script} @ONLY)

    if(runConanAutomatically OR NOT EXISTS ${CMAKE_BINARY_DIR}/conan_paths.cmake)
        nx_execute_process_or_fail(
            COMMAND ${CMAKE_COMMAND}
                -DinstallSystemRequirements=${installSystemRequirements}
                -P ${run_conan_script}
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

nx_init_conan()
