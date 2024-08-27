## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

# This file contains functions for populating the script for ninja_tool with commands to be
# executed on a pre-build step.
option(runNinjaTool
    "Invoke ninja_tool.py (via ninja_launcher) before each invocation of ninja." ON)

set(ninja_tool_script_name "${CMAKE_BINARY_DIR}/pre_build.ninja_tool")

function(nx_setup_ninja_preprocessor)
    if(CMAKE_GENERATOR STREQUAL "Ninja")
        if(runNinjaTool)
            # Prevent runing ninja_tool on generation stage.
            set(ENV{DISABLE_NINJA_TOOL} "true")

            set(launcher_name "${open_build_utils_dir}/ninja/ninja_launcher")

            if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Windows")
                string(APPEND launcher_name ".bat")
            else()
                string(APPEND launcher_name ".sh")
            endif()
        else()
            find_program(launcher_name ninja)
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
            nx_shell_escape_string("${target}" escaped_target)
        else()
            file(RELATIVE_PATH file ${CMAKE_BINARY_DIR} "${target}")
            # In build.ninja targets are written in the native format (e.g. with "/" as a directory
            # separator on Windows), so we have to use the same format in pre_build.ninja_tool.
            file(TO_NATIVE_PATH "${file}" native_file)
            nx_shell_escape_string("${native_file}" escaped_target)
        endif()

        set_property(
            GLOBAL APPEND_STRING PROPERTY pre_build_commands "strengthen ${escaped_target}\n\n")
    endforeach()
endfunction()

function(nx_save_ninja_preprocessor_script)
    get_property(targets GLOBAL PROPERTY pre_build_commands)
    file(WRITE ${ninja_tool_script_name} ${targets})
endfunction()

function(nx_add_affected_targets_list_generation sha)
    nx_execute_process_or_fail(
        COMMAND git -C "${CMAKE_CURRENT_LIST_DIR}" diff --name-only ${sha}
        OUTPUT_FILE "${CMAKE_BINARY_DIR}/${NX_CHANGED_FILES_LIST_FILE_NAME}"
        ERROR_MESSAGE "Cannot obtain changed files list between commit ${sha} and working tree.")

    set_property(GLOBAL APPEND_STRING PROPERTY
        pre_build_commands
        "generate_affected_targets_list \"${CMAKE_SOURCE_DIR}\" "
        "\"${NX_CHANGED_FILES_LIST_FILE_NAME}\" \"${NX_AFFECTED_TARGETS_LIST_FILE_NAME}\"\n\n")
endfunction()

nx_setup_ninja_preprocessor()
