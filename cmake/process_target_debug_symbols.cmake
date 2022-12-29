## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

function(nx_process_macos_target_debug_symbols target)
    get_target_property(type ${target} TYPE)
    if(NOT type MATCHES "SHARED_LIBRARY|EXECUTABLE")
        return()
    endif()

    if(NOT CMAKE_STRIP)
        message(FATAL_ERROR "strip is not found.")
    endif()

    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND dsymutil $<TARGET_FILE:${target}> -o $<TARGET_FILE:${target}>.dSYM
        COMMAND ${CMAKE_STRIP} -S $<TARGET_FILE:${target}>
        COMMENT "Stripping ${target}")
endfunction()
