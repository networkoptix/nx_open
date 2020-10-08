# This file contains functions for populating the script for ninja_tool with commands to be
# executed on a pre-build step.
include(CMakeParseArguments)

set(ninja_tool_script_name "${CMAKE_BINARY_DIR}/pre_build.ninja_tool")

function(nx_setup_ninja_preprocessor)
    if(CMAKE_GENERATOR STREQUAL "Ninja")
        set(launcher_name "${CMAKE_SOURCE_DIR}/build_utils/ninja/ninja_launcher")

        if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Windows")
            string(APPEND launcher_name ".bat")
        endif()

        set(CMAKE_MAKE_PROGRAM ${launcher_name}
            CACHE FILEPATH "Path to ninja launcher"
            FORCE)

        get_property(property_set GLOBAL PROPERTY pre_build_commands SET)

        # Always add "clean" command to the script.
        set_property(GLOBAL PROPERTY pre_build_commands "clean\n\n")

        # Remove old pre_build.ninja_tool file.
        file(REMOVE ${ninja_tool_script_name})
    endif()
endfunction()

function(nx_add_targets_to_strengthened)
    foreach(target ${ARGN})
        if(TARGET ${target})
            set_property(
                GLOBAL APPEND_STRING PROPERTY pre_build_commands "strengthen ${target}\n\n")
        else()
            file(RELATIVE_PATH file ${CMAKE_BINARY_DIR} "${target}")
            set_property(
                GLOBAL APPEND_STRING PROPERTY pre_build_commands "strengthen ${file}\n\n")
        endif()
    endforeach()
endfunction()

function(nx_add_custom_pre_build_command command)
    set_property(GLOBAL APPEND_STRING PROPERTY pre_build_commands "run ${command}\n\n")
endfunction()

function(nx_use_custom_verify_globs)
    set_property(GLOBAL APPEND_STRING
        PROPERTY pre_build_commands "substitute_verify_globs ${verify_globs_directory}\n\n")
endfunction()

function(nx_add_pre_build_artifacts_check)
    # TODO: Modify ninja_tool `run` to allow it run programs with custom environment variables set.
    # The preffered syntax in "pre_build.ninja_tool" file would be the following:
    # run [<ENV-VARIABLE1>=<VALUE1> [...<ENV-VARIABLEN>=<VALUEN>]] <COMMAND> [<ARG1> [...<ARGN>]]
    nx_c_escape_string("${CMAKE_SOURCE_DIR}/build_utils/python/run_after_fetch.py"
        run_after_fetch_escaped)
    nx_c_escape_string("${packages_sync_flag_file}" packages_sync_flag_file_escaped)
    nx_c_escape_string("${CMAKE_SOURCE_DIR}/build_utils/python" tools_dir_escaped)
    nx_c_escape_string("${CMAKE_SOURCE_DIR}" sync_dependencies_path_escaped)
    nx_c_escape_string("${PYTHON_EXECUTABLE}" python_executable_escaped)
    nx_c_escape_string("${PACKAGES_DIR}" package_dir_escaped)
    nx_c_escape_string("${cmake_include_file}" cmake_include_file_escaped)
    set(check_artifacts_command
        "${python_executable_escaped} ${run_after_fetch_escaped}"
        " --checker-source-dir=${CMAKE_SOURCE_DIR}"
        " --checker-flag-file=${packages_sync_flag_file_escaped}"
        " --checker-run-script=sync_dependencies"
        " --checker-pythonpath ${tools_dir_escaped} ${sync_dependencies_path_escaped}"
        " --packages-dir=${package_dir_escaped}"
        " --target=${rdep_target}"
        " --release-version=${releaseVersion}"
        " --customization=${customization}"
        " --cmake-include-file=${cmake_include_file_escaped}"
        " --options sync-timestamps=True"
        " --verbose")
    nx_add_custom_pre_build_command("${check_artifacts_command}")
endfunction()

function(nx_save_ninja_preprocessor_script)
    get_property(targets GLOBAL PROPERTY pre_build_commands)
    file(WRITE ${ninja_tool_script_name} ${targets})
endfunction()
