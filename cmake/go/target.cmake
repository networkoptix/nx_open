## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

include_guard()

include(go/compiler)
include(go/utils)

# Given a folder structure like /service_name/cmd, with cmd containing one folder per executable,
# nx_go_add_target will gather the list of executables (ommitting "internal"), build them, create
# a target for each one with the same name, and drop them into ${CMAKE_BINARY_DIR}/bin. It must be
# invoked inside of the go project root directory.
#
# Args:
#  - target: the name of the target to create.
#  - FOLDER <folder> (optional): the IDE folder to assign the target to.
#  - EXECUTABLES_VAR <executable_targets> (optional): output variable containing the list of
#    executable targets created.
#
# Usage:
#  - nx_go_add_target(my_service FOLDER cloud EXECUTABLES_VAR my_exec_targets_list) or
#  - nx_go_add_target(my_service)
function(nx_go_add_target target)
    set(one_value_args FOLDER EXECUTABLES_VAR)
    cmake_parse_arguments(GO_TARGET "" "${one_value_args}" "" ${ARGN})

    add_custom_target(${target})

    if(GO_TARGET_FOLDER)
        set_target_properties(${target} PROPERTIES FOLDER ${GO_TARGET_FOLDER})
    endif()

    list_subdirectories(${CMAKE_CURRENT_SOURCE_DIR}/cmd executables)
    list(REMOVE_ITEM executables internal)

    foreach(executable ${executables})
        nx_go_build(
            ${executable}
            ${CMAKE_CURRENT_SOURCE_DIR}
            ./cmd/${executable}
            FOLDER ${GO_TARGET_FOLDER}
        )
        add_dependencies(${target} ${executable})
    endforeach()

    if(GO_TARGET_EXECUTABLES_VAR)
        set(${GO_TARGET_EXECUTABLES_VAR} ${executables} PARENT_SCOPE)
    endif()
endfunction()

# Creates a target named <target> for Golang compilation. <target> should be a folder
# containing a .go file with the main package. It must be invoked inside of the go project
# root directory.
#
# Args:
#  - target: the name of the target to create and build.
#
# Usage:
#  - nx_go_build(./cmd/service_main)
function(nx_go_build target working_dir package_path)
    set(one_value_args FOLDER)
    cmake_parse_arguments(GO_BUILD "" "${one_value_args}" "" ${ARGN})

    add_custom_target(${target})

    if(GO_BUILD_FOLDER)
        set_target_properties(${target} PROPERTIES FOLDER ${GO_BUILD_FOLDER})
    endif()

    nx_go_fix_target_exe(${target} target_exe)

    add_custom_command(
        TARGET ${target}
        WORKING_DIRECTORY ${working_dir}
        COMMAND ${NX_GO_COMPILER} build ${package_path}
        COMMAND ${CMAKE_COMMAND} -E copy ${target_exe} ${CMAKE_BINARY_DIR}/bin
        COMMAND ${CMAKE_COMMAND} -E remove -f ${target_exe}
    )
endfunction()
