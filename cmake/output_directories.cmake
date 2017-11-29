function(set_output_directories)
    cmake_parse_arguments(DIR "" "RUNTIME;LIBRARY;AFFECTED_VARIABLES_RESULT" "" ${ARGN})

    if(DIR_RUNTIME)
        set(DIR_RUNTIME "/${DIR_RUNTIME}")
    endif()
    if(DIR_LIBRARY)
        set(DIR_LIBRARY "/${DIR_LIBRARY}")
    endif()

    if(CMAKE_MULTI_CONFIGURATION_MODE)
        set(configs ${CMAKE_ACTIVE_CONFIGURATIONS})
    else()
        set(configs " ")
    endif()

    set(affected_variables)

    foreach(config IN LISTS configs)
        if(config STREQUAL " ")
            set(CONFIG "")
            set(base "${CMAKE_BINARY_DIR}")
        else()
            string(TOUPPER ${config} CONFIG)
            set(CONFIG "_${CONFIG}")
            set(base "${CMAKE_BINARY_DIR}/${config}")
        endif()

        file(MAKE_DIRECTORY "${base}${DIR_RUNTIME}")
        file(MAKE_DIRECTORY "${base}${DIR_LIBRARY}")

        set(CMAKE_RUNTIME_OUTPUT_DIRECTORY${CONFIG} "${base}${DIR_RUNTIME}" PARENT_SCOPE)
        list(APPEND affected_variables "CMAKE_RUNTIME_OUTPUT_DIRECTORY${CONFIG}")
        set(CMAKE_LIBRARY_OUTPUT_DIRECTORY${CONFIG} "${base}${DIR_LIBRARY}" PARENT_SCOPE)
        list(APPEND affected_variables "CMAKE_LIBRARY_OUTPUT_DIRECTORY${CONFIG}")
    endforeach()

    set(distribution_output_dir "${CMAKE_BINARY_DIR}/distrib")
    file(MAKE_DIRECTORY ${distribution_output_dir})
    set(distribution_output_dir ${distribution_output_dir} PARENT_SCOPE)

    if(DIR_AFFECTED_VARIABLES_RESULT)
        set(${DIR_AFFECTED_VARIABLES_RESULT} ${affected_variables} PARENT_SCOPE)
    endif()
endfunction()

set_output_directories(RUNTIME "bin" LIBRARY "lib")
