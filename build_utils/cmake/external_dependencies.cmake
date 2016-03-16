function(get_package_version package result)
    set(${result} ${${package}.version} PARENT_SCOPE)
endfunction()

function(get_external_dependencies)
    set(args --target ${TARGET_TYPE}
             --target-dir ${CMAKE_BINARY_DIR}/target
             --deps-file cmake)
    if(NOT CMAKE_BUILD_TYPE MATCHES Release)
        list(APPEND args --debug)
    endif()

    foreach(package ${ARGN})
        set(version ${${package}.version})
        if(version)
            list(APPEND args ${package}-${version})
        else()
            list(APPEND args ${package})
        endif()
    endforeach()

    execute_process(
            COMMAND ${PYTHON_EXECUTABLE}
                    ${PYTHON_MODULES_DIR}/rdep_dependencies.py
                    ${args}
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
            RESULT_VARIABLE result)

    if(NOT ${result} EQUAL 0)
        message(FATAL_ERROR "Could not get dependencies.")
    endif()
endfunction()
