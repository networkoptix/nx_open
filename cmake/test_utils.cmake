enable_testing()

set(testTempDirectory "${CMAKE_BINARY_DIR}" CACHE STRING "Temp directory for running tests.")

function(nx_add_test target)
    set_output_directories(RUNTIME "bin" LIBRARY "lib"
        AFFECTED_VARIABLES_RESULT affected_variables)
    nx_expose_variables_to_parent_scope(affected_variables)

    nx_add_target(${target} EXECUTABLE ${ARGN})

    add_test(NAME ${target}
        COMMAND
            ${target} --tmp ${testTempDirectory}
    )

    if(WINDOWS)
        set_tests_properties(${target} PROPERTIES ENVIRONMENT "PATH=${QT_DIR}/bin\;$ENV{PATH}")
    endif()
endfunction()
