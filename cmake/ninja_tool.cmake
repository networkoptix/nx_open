# This file contains functions for populating the script for ninja_tool with commands to be
# executed on a pre-build step.
include(CMakeParseArguments)

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
        set_property(GLOBAL PROPERTY pre_build_commands "clean\n")
    endif()
endfunction()

function(nx_add_targets_to_strengthened)
    foreach(target ${ARGN})
        if(TARGET ${target})
            set_property(
                GLOBAL APPEND_STRING PROPERTY pre_build_commands "strengthen ${target}\n")
        else()
            file(RELATIVE_PATH file ${CMAKE_BINARY_DIR} "${target}")
            set_property(
                GLOBAL APPEND_STRING PROPERTY pre_build_commands "strengthen ${file}\n")
        endif()
    endforeach()
endfunction()

function(nx_add_custom_pre_build_command command)
    set_property(GLOBAL APPEND_STRING PROPERTY pre_build_commands "run ${command}\n")
endfunction()

function(nx_use_custom_verify_globs)
    set_property(GLOBAL APPEND_STRING PROPERTY pre_build_commands "substitute_verify_globs\n")
endfunction()

function(nx_save_ninja_preprocessor_script)
    get_property(targets GLOBAL PROPERTY pre_build_commands)
    file(WRITE ${CMAKE_BINARY_DIR}/pre_build.ninja ${targets})
endfunction()
