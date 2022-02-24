## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

include(CMakeParseArguments)

function(nx_to_python_compatible_bool var)
    if(${var})
        set(${var} "True" PARENT_SCOPE)
    else()
        set(${var} "False" PARENT_SCOPE)
    endif()
endfunction()

function(nx_to_cpp_compatible_bool var)
    if(${var})
        set(${var} "true" PARENT_SCOPE)
    else()
        set(${var} "false" PARENT_SCOPE)
    endif()
endfunction()

function(nx_option option_name)
    option(${option_name} ${ARGN})
    nx_to_cpp_compatible_bool(${option_name})
    set(${option_name} ${${option_name}} PARENT_SCOPE)
endfunction()
