function(nx_fail_with_invalid_parameters)
    cmake_parse_arguments(FAIL "" "MESSAGE;DESCRIPTION" "" ${ARGN})

    if(NOT FAIL_MESSAGE)
        set(FAIL_MESSAGE "Incompatible parameters:")
    endif()

    foreach(var ${FAIL_UNPARSED_ARGUMENTS})
        string(APPEND FAIL_MESSAGE "\n  ${var} = ${${var}}")
    endforeach()

    if(FAIL_DESCRIPTION)
        string(APPEND FAIL_MESSAGE "\n${FAIL_DESCRIPTION}")
    endif()

    message(FATAL_ERROR ${FAIL_MESSAGE})
endfunction()

if(targetDevice STREQUAL edge1 AND NOT customization STREQUAL "digitalwatchdog")
    nx_fail_with_invalid_parameters(targetDevice customization
        DESCRIPTION "edge1 can be used only with digitalwatchdog customization.")
endif()
