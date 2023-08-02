## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

include_guard()

include(list_subdirectories)
include(go/utils)

if(NOT cmake_nxgtest_dir)
    set(cmake_nxgtest_dir ${CMAKE_CURRENT_LIST_DIR})
endif()

# When invoked in a go module directory, recursively locates each test package, builds it, and
# places the binary in ${CMAKE_BINARY_DIR}/bin. The given target name will have a dependency on
# each test binary that is built. The effect of building this top level test target is to build all
# discovered test packages and have them dropped into the ${CMAKE_BINARY_DIR}/bin.
# NOTE: withTests cmake variable must be defined for tests to be added.
function(nx_go_add_test target)
    if (NOT withTests)
        return()
    endif()

    set(one_value_args FOLDER)
    set(multi_value_args EXCLUDE_FOLDERS DEPENDS)
    cmake_parse_arguments(GO_TEST "" "${one_value_args}" "${multi_value_args}" ${ARGN})

    add_custom_target(${target})
    set_target_properties(${target} PROPERTIES FOLDER ${GO_TEST_FOLDER})

    nx_go_list_test_packages(${CMAKE_CURRENT_SOURCE_DIR} package_paths)

    foreach(path ${package_paths})
        if ("${path}" IN_LIST GO_TEST_EXCLUDE_FOLDERS)
            continue()
        endif()

        string(REPLACE "/" ";" path_list ${path})
        list(POP_BACK path_list package)

        # NOTE: CI server looks for test executables like so:
        # find . -name '*_ut' -or -name '*_ut.exe' -or -name '*_tests' -or -name '*_tests.exe'
        # So, easiest thing to do is append _ut to the target name.
        if("." STREQUAL "${path}")
            # "." means current directory has tests, appending package name to it won't work.
            set(target_ut ${target}_ut)
        else()
            set(target_ut ${target}_${package}_ut)
        endif()

        if(NX_TEST_JIRA_PROJECT)
            nx_store_test_metainformation(${target_ut})
        endif()

        nx_go_build_test(
            ${target_ut}
            ${CMAKE_CURRENT_SOURCE_DIR}
            ./${path}
            FOLDER ${GO_TEST_FOLDER}
            DEPENDS ${GO_TEST_DEPENDS}
        )

        add_dependencies(${target} ${target_ut})
    endforeach()
endfunction()

function(nx_go_build_test target working_dir package_path)
    set(one_value_args FOLDER)
    set(multi_value_args DEPENDS)
    cmake_parse_arguments(GO_BUILD_TEST "" "${one_value_args}" "${multi_value_args}" ${ARGN})

    nx_go_fix_target_exe(${target} target_exe)
    set(target_path ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${target_exe})

    set(compile_command ${NX_GO_COMPILER} test -c ${package_path} -o ${target_exe})

    file(GLOB_RECURSE test_source_files FOLLOW_SYMLINKS "${package_path}/*.go")

    if(WIN32)
        list(PREPEND compile_command ${CMAKE_COMMAND} -E env PATH="${CONAN_MINGW-W64_ROOT}/bin")
    endif()

    add_custom_command(
        OUTPUT ${target_path}
        WORKING_DIRECTORY ${working_dir}
        DEPENDS ${test_source_files} ${GO_BUILD_TEST_DEPENDS}
        COMMAND ${compile_command}
        COMMAND ${CMAKE_COMMAND} -E copy ${target_exe} ${target_path}
        COMMAND ${CMAKE_COMMAND} -E remove -f ${target_exe}
    )

    nx_find_files(go_sources ${package_path} "go")
    add_custom_target(${target} ALL SOURCES ${go_sources} DEPENDS ${target_path})

    if(GO_BUILD_TEST_FOLDER)
        set_target_properties(${target} PROPERTIES FOLDER ${GO_BUILD_TEST_FOLDER})
    endif()

    nx_go_fix_target_exe(${target} target_exe)

    if (WIN32)
        add_custom_command(
            TARGET ${target}
            WORKING_DIRECTORY ${working_dir}
            DEPENDS ${GO_BUILD_TEST_DEPENDS}
            COMMAND COMMAND ${CMAKE_COMMAND} -E env PATH="${CONAN_MINGW-W64_ROOT}/bin" ${NX_GO_COMPILER} test -c ${package_path} -o ${target_exe}
            COMMAND ${CMAKE_COMMAND} -E copy ${target_exe} ${CMAKE_BINARY_DIR}/bin
            COMMAND ${CMAKE_COMMAND} -E remove -f ${target_exe}
        )
    else()
        add_custom_command(
            TARGET ${target}
            WORKING_DIRECTORY ${working_dir}
            DEPENDS ${GO_BUILD_TEST_DEPENDS}
            COMMAND ${NX_GO_COMPILER} test -c ${package_path} -o ${target_exe}
            COMMAND ${CMAKE_COMMAND} -E copy ${target_exe} ${CMAKE_BINARY_DIR}/bin
            COMMAND ${CMAKE_COMMAND} -E remove -f ${target_exe}
        )
    endif()

    add_test(NAME ${target} COMMAND ${CMAKE_BINARY_DIR}/bin/${target_exe})

    # NOTE: unit_tests is a custom target set in open/cmake/test_utils.cmake
    add_dependencies(unit_tests ${target})
endfunction()

function(nx_go_list_test_packages src_dir out_packages)
    set(packages)

    file(GLOB files ${src_dir}/*_test.go)
    if(files)
        list(APPEND packages ".")
    endif()

    list_subdirectories(${src_dir} dirs RECURSE)
    foreach(dir ${dirs})
        file(GLOB files ${src_dir}/${dir}/*_test.go)
        if(files)
            list(APPEND packages ${dir})
        endif()
    endforeach()

    set(${out_packages} ${packages} PARENT_SCOPE)
endfunction()
