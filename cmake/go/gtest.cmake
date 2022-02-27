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
    cmake_parse_arguments(GO_TEST "" "${one_value_args}" "" ${ARGN})

    add_custom_target(${target})
    set_target_properties(${target} PROPERTIES FOLDER ${GO_TEST_FOLDER})

    nx_go_list_test_packages(${CMAKE_CURRENT_SOURCE_DIR} package_paths)

    foreach(path ${package_paths})
        string(REPLACE "/" ";" path_list ${path})
        list(POP_BACK path_list package)

        # NOTE: CI server looks for test executables like so:
        # find . -name '*_ut' -or -name '*_ut.exe' -or -name '*_tests' -or -name '*_tests.exe'
        # So, easiest thing to do is append _ut to the target name.
        set(target_ut ${target}_${package}_ut)

        nx_go_build_test(
            ${target_ut}
            ${CMAKE_CURRENT_SOURCE_DIR}
            ./${path}
            FOLDER ${GO_TEST_FOLDER}
        )

        add_dependencies(${target} ${target_ut})
    endforeach()
endfunction()

function(nx_go_build_test target working_dir package_path)
    set(one_value_args FOLDER)
    cmake_parse_arguments(GO_BUILD_TEST "" "${one_value_args}" "" ${ARGN})

    add_custom_target(${target})

    if(GO_BUILD_TEST_FOLDER)
        set_target_properties(${target} PROPERTIES FOLDER ${GO_BUILD_TEST_FOLDER})
    endif()

    nx_go_fix_target_exe(${target} target_exe)

    add_custom_command(
        TARGET ${target}
        WORKING_DIRECTORY ${working_dir}
        COMMAND ${NX_GO_COMPILER} test -c ${package_path} -o ${target_exe}
        COMMAND ${CMAKE_COMMAND} -E copy ${target_exe} ${CMAKE_BINARY_DIR}/bin
        COMMAND ${CMAKE_COMMAND} -E remove -f ${target_exe}
    )

    add_test(NAME ${target} COMMAND ${CMAKE_BINARY_DIR}/bin/${target_exe})

    # NOTE: unit_tests is a custom target set in open/cmake/test_utils.cmake
    add_dependencies(unit_tests ${target})
endfunction()

function(nx_go_list_test_packages src_dir out_packages)
    list_subdirectories(${src_dir} dirs RECURSE)

    set(packages)
    foreach(dir ${dirs})
        file(GLOB files ${src_dir}/${dir}/*_test.go)
        if(files)
            list(APPEND packages ${dir})
        endif()
    endforeach()

    set(${out_packages} ${packages} PARENT_SCOPE)
endfunction()
