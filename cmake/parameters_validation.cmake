function(nx_fail_with_invalid_parameters)
    cmake_parse_arguments(FAIL "" "MESSAGE" "" ${ARGN})

    if(NOT FAIL_MESSAGE)
        set(FAIL_MESSAGE "Incompatible parameters:")
    endif()

    foreach(var ${FAIL_UNPARSED_ARGUMENTS})
        string(APPEND FAIL_MESSAGE "\n  ${var} = ${${var}}")
    endforeach()

    message(FATAL_ERROR ${FAIL_MESSAGE})
endfunction()

if(targetDevice STREQUAL edge1 AND customization STREQUAL "hanwha")
    nx_fail_with_invalid_parameters(targetDevice customization)
endif()
