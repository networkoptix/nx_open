enable_testing()

add_custom_target(unit_tests)
set_target_properties(unit_tests PROPERTIES FOLDER tests)

set(testTempDirectory "${CMAKE_BINARY_DIR}" CACHE STRING "Temp directory for running tests.")

function(nx_add_test target) # [NO_GTEST] ...
    cmake_parse_arguments(NX_ADD_TEST "NO_GTEST"
        #[[one_value_keywords]] "" #[[multi_value_keywords]] "" ${ARGN})

    set_output_directories(RUNTIME "bin" LIBRARY "lib"
        AFFECTED_VARIABLES_RESULT affected_variables)
    nx_expose_variables_to_parent_scope(${affected_variables})

    if(NOT NX_ADD_TEST_NO_GTEST)
        set(additional_private_libs
            PRIVATE_LIBS
                GTest
                GMock
        )
    endif()

    nx_add_target(${target}
        EXECUTABLE
        NO_RC_FILE
        NO_MOC
        ${additional_private_libs}
        FOLDER tests
        ${NX_ADD_TEST_UNPARSED_ARGUMENTS}
    )

    if(NOT NX_ADD_TEST_NO_GTEST)
        nx_force_include(${target}
            ${CMAKE_SOURCE_DIR}/libs/nx_utils/src/nx/utils/test_support/custom_gtest_printers.h)
    endif()

    add_test(NAME ${target} COMMAND ${target})

    if(WINDOWS)
        # Adding ${CMAKE_MSVCIDE_RUN_PATH} to PATH for running unit tests via CTest.
        set(new_path "${CMAKE_MSVCIDE_RUN_PATH};$ENV{PATH}")
        string(REPLACE ";" "\;" new_path "${new_path}")
        set_tests_properties(${target} PROPERTIES ENVIRONMENT "PATH=${new_path}")
    endif()

    add_dependencies(unit_tests ${target})
endfunction()

function(nx_add_server_plugin_test target) # [NO_GTEST]
    set(CMAKE_INSTALL_RPATH "$ORIGIN/../lib:$ORIGIN/plugins:$ORIGIN/plugins_optional")

    list(APPEND CMAKE_MSVCIDE_RUN_PATH
        ${CMAKE_BINARY_DIR}/bin/plugins
        ${CMAKE_BINARY_DIR}/bin/plugins_optional
    )

    nx_add_test(${ARGV})

    if(WINDOWS)
        # Include "ut_msvc.user.props" into each Visual Studio project.
        set_target_properties(${target} PROPERTIES
            VS_USER_PROPS ${CMAKE_BINARY_DIR}/plugins/ut_msvc.user.props)
    endif()
endfunction()
