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
#  - C_GO_INCLUDE_DIRECTORY <directory> (optional): directory for header files generated by cgo.
#  - DEPENDS <list> (optional): additional build dependencies.
#
# Usage:
#  - nx_go_add_target(my_service FOLDER cloud EXECUTABLES_VAR my_exec_targets_list) or
#  - nx_go_add_target(my_service)
function(nx_go_add_target target)
    set(one_value_args FOLDER EXECUTABLES_VAR C_GO_INCLUDE_DIRECTORY)
    set(multi_value_args DEPENDS)
    cmake_parse_arguments(GO_TARGET "" "${one_value_args}" "${multi_value_args}" ${ARGN})

    add_custom_target(${target})

    if(GO_TARGET_FOLDER)
        set_target_properties(${target} PROPERTIES FOLDER ${GO_TARGET_FOLDER})
    endif()

    file(GLOB_RECURSE GO_SRC_FILES ${CMAKE_CURRENT_SOURCE_DIR} *.go)
    set(GO_TARGET_DEPENDS ${GO_TARGET_DEPENDS} ${GO_SRC_FILES})

    list_subdirectories(${CMAKE_CURRENT_SOURCE_DIR}/cmd executables)
    list(REMOVE_ITEM executables internal)

    foreach(executable ${executables})
        nx_go_build(
            ${executable}
            ${CMAKE_CURRENT_SOURCE_DIR}
            ./cmd/${executable}
            FOLDER ${GO_TARGET_FOLDER}
            DEPENDS ${GO_TARGET_DEPENDS}
            C_GO_INCLUDE_DIRECTORY "${GO_TARGET_C_GO_INCLUDE_DIRECTORY}"
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
    set(one_value_args FOLDER C_GO_INCLUDE_DIRECTORY)
    set(multi_value_args DEPENDS)
    cmake_parse_arguments(GO_BUILD "" "${one_value_args}" "${multi_value_args}" ${ARGN})

    nx_go_fix_target_exe(${target} target_exe)

    add_custom_target(${target} ALL DEPENDS ${CMAKE_BINARY_DIR}/bin/${target_exe})
    if(GO_BUILD_FOLDER)
        set_target_properties(${target} PROPERTIES FOLDER ${GO_BUILD_FOLDER})
    endif()
    if (WIN32)
        add_custom_command(
            OUTPUT ${CMAKE_BINARY_DIR}/bin/${target_exe}
            WORKING_DIRECTORY ${working_dir}
            DEPENDS ${GO_BUILD_DEPENDS}
            COMMAND ${CMAKE_COMMAND} -E env PATH="${CONAN_MINGW-W64_ROOT}/bin" ${NX_GO_COMPILER} build ${package_path}
            COMMAND ${CMAKE_COMMAND} -E copy ${target_exe} ${CMAKE_BINARY_DIR}/bin
            COMMAND ${CMAKE_COMMAND} -E remove -f ${target_exe}
        )
    else()
        add_custom_command(
            OUTPUT ${CMAKE_BINARY_DIR}/bin/${target_exe}
            WORKING_DIRECTORY ${working_dir}
            DEPENDS ${GO_BUILD_DEPENDS}
            COMMAND ${NX_GO_COMPILER} build ${package_path}
            COMMAND ${CMAKE_COMMAND} -E copy ${target_exe} ${CMAKE_BINARY_DIR}/bin
            COMMAND ${CMAKE_COMMAND} -E remove -f ${target_exe}
        )
    endif()

    if(GO_BUILD_C_GO_INCLUDE_DIRECTORY)
        file(GLOB_RECURSE all_go_files FOLLOW_SYMLINKS "${working_dir}/*.go")

        string(APPEND CMAKE_CXX_FLAGS " -Wno-unused-parameter")

        if(WIN32)
            set(target_lib "${target}.lib")
            set(target_dll "${target}.dll")
            set(full_lib_path "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${target_lib}")
            set(full_dll_path "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${target_dll}")
            set(deps "${full_lib_path}" "${full_dll_path}")
            add_custom_command(
                OUTPUT "${full_lib_path}" "${full_dll_path}" "${GO_BUILD_C_GO_INCLUDE_DIRECTORY}/${target}.h"
                WORKING_DIRECTORY ${working_dir}
                DEPENDS ${all_go_files} ${GO_BUILD_DEPENDS}
                COMMAND ${CMAKE_COMMAND} -E make_directory "${GO_BUILD_C_GO_INCLUDE_DIRECTORY}"
                COMMAND ${CMAKE_COMMAND} -E env PATH="${CONAN_MINGW-W64_ROOT}/bin" CC=gcc ${NX_GO_COMPILER} build -o "${CMAKE_CURRENT_BINARY_DIR}/${target_dll}" -buildmode=c-shared ${package_path}
                COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_BINARY_DIR}/${target_dll}" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${target_dll}"
                COMMAND ${CMAKE_COMMAND} -E remove -f "${CMAKE_CURRENT_BINARY_DIR}/${target_dll}"
                COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_BINARY_DIR}/${target}.h" "${GO_BUILD_C_GO_INCLUDE_DIRECTORY}/${target}.h"
                COMMAND ${CMAKE_COMMAND} -E remove -f "${CMAKE_CURRENT_BINARY_DIR}/${target}.h"
                COMMAND ${CMAKE_COMMAND} -E env PATH="${CONAN_MINGW-W64_ROOT}/bin" gendef "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${target_dll}"
                COMMAND ${CMAKE_COMMAND} -E env PATH="${CONAN_MINGW-W64_ROOT}/bin" dlltool -d ${target}.def -l "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${target_lib}" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${target_dll}"
                COMMAND ${CMAKE_COMMAND} -E remove -f ${target}.def
            )
        else()
            set(target_lib "lib${target}.so")
            set(full_lib_path "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${target_lib}")
            set(deps "${full_lib_path}")
            add_custom_command(
                OUTPUT "${full_lib_path}" "${GO_BUILD_C_GO_INCLUDE_DIRECTORY}/${target}.h"
                WORKING_DIRECTORY ${working_dir}
                DEPENDS ${all_go_files} ${GO_BUILD_DEPENDS}
                COMMAND ${CMAKE_COMMAND} -E make_directory "${GO_BUILD_C_GO_INCLUDE_DIRECTORY}"
                COMMAND ${CMAKE_COMMAND} -E env CC=${CMAKE_C_COMPILER} CGO_CFLAGS=${CMAKE_CXX_FLAGS} CGO_LDFLAGS=${CMAKE_C_FLAGS}${CMAKE_SHARED_LINKER_FLAGS} ${NX_GO_COMPILER} build -o "${CMAKE_CURRENT_BINARY_DIR}/${target_lib}" -buildmode=c-shared ${package_path}
                COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_BINARY_DIR}/${target_lib}" "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${target_lib}"
                COMMAND ${CMAKE_COMMAND} -E remove -f "${CMAKE_CURRENT_BINARY_DIR}/${target_lib}"
                COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_BINARY_DIR}/lib${target}.h" "${GO_BUILD_C_GO_INCLUDE_DIRECTORY}/${target}.h"
                COMMAND ${CMAKE_COMMAND} -E remove -f "${CMAKE_CURRENT_BINARY_DIR}/lib${target}.h"
            )
        endif()
        set_source_files_properties("${GO_BUILD_C_GO_INCLUDE_DIRECTORY}/${target}.h" PROPERTIES GENERATED TRUE)
        add_custom_target(${target}_lib_build ALL DEPENDS ${deps})
        if(GO_BUILD_FOLDER)
            set_target_properties(${target}_lib_build PROPERTIES FOLDER ${GO_BUILD_FOLDER})
        endif()

        add_library(${target}_lib INTERFACE)
        if(GO_BUILD_FOLDER)
            set_target_properties(${target}_lib PROPERTIES FOLDER ${GO_BUILD_FOLDER})
        endif()
        add_dependencies(${target}_lib ${target}_lib_build)
        target_link_libraries(${target}_lib INTERFACE "${full_lib_path}")
        add_dependencies(${target} ${target}_lib)
    endif()
endfunction()

function(nx_go_openapi_gen_command source_yaml target_go working_dir)
    set(multi_value_args DEPENDS)
    cmake_parse_arguments(GO_OAPI_GEN "" "" "${multi_value_args}" ${ARGN})

    set(gobin ${CMAKE_CURRENT_BINARY_DIR})
    get_filename_component(target_dir "${target_go}" DIRECTORY)
    add_custom_command(
        OUTPUT "${target_go}"
        DEPENDS
            "${source_yaml}"
            ${GO_OAPI_GEN_DEPENDS}
        WORKING_DIRECTORY "${working_dir}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${target_dir}"
        COMMAND ${CMAKE_COMMAND} -E env GOBIN=${gobin} ${NX_GO_COMPILER} install github.com/deepmap/oapi-codegen/cmd/oapi-codegen@v1.9.0
        COMMAND ${gobin}/oapi-codegen -package handlers -generate server,types -o "${target_go}" "${source_yaml}")
    set_source_files_properties("${target_go}" PROPERTIES GENERATED TRUE)
endfunction()

function(nx_go_add_rest_api_service name working_dir)
    set(option_args WITHOUT_TESTS)
    set(one_value_args C_GO_INCLUDE_DIRECTORY)
    set(multi_value_args DEPENDS TEST_EXCLUDE_FOLDERS OAPI_YAMLS GENERATED_FILES)
    cmake_parse_arguments(GO_REST_SERVICE "${option_args}" "${one_value_args}" "${multi_value_args}" ${ARGN})

    nx_go_add_target(
        "${name}"
        DEPENDS
            ${GO_REST_SERVICE_GENERATED_FILES}
            ${GO_REST_SERVICE_DEPENDS}
        C_GO_INCLUDE_DIRECTORY "${GO_REST_SERVICE_C_GO_INCLUDE_DIRECTORY}"
        FOLDER cloud/utility
    )

    list(LENGTH GO_REST_SERVICE_OAPI_YAMLS len)
    math(EXPR lenMinusOne "${len} - 1")
    foreach(i RANGE ${lenMinusOne})
        list(GET GO_REST_SERVICE_OAPI_YAMLS ${i} yaml)
        list(GET GO_REST_SERVICE_GENERATED_FILES ${i} go_file)
        nx_go_openapi_gen_command(
            "${yaml}"
            "${go_file}"
            "${working_dir}"
        )
    endforeach()

    if(NOT GO_REST_SERVICE_WITHOUT_TESTS)
        nx_go_add_test(
            "${name}_ut"
            FOLDER cloud/tests
            EXCLUDE_FOLDERS ${GO_REST_SERVICE_TEST_EXCLUDE_FOLDERS}
            DEPENDS
                ${GO_REST_SERVICE_GENERATED_FILES}
                ${GO_REST_SERVICE_DEPENDS}
        )
    endif()
endfunction()
