## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

function(nx_process_macos_target_debug_symbols target)
    get_target_property(type ${target} TYPE)
    if(NOT type MATCHES "SHARED_LIBRARY|EXECUTABLE")
        return()
    endif()

    if(NOT CMAKE_STRIP)
        message(FATAL_ERROR "strip is not found.")
    endif()

    set(oneValueArgs OUTPUT_DIR)
    cmake_parse_arguments(DSYM "" "${oneValueArgs}" "" ${ARGN})

    if(DSYM_OUTPUT_DIR)
        set(create_dsym_command COMMAND dsymutil $<TARGET_FILE:${target}> -o "${DSYM_OUTPUT_DIR}/${target}.dSYM")
    else()
        set(create_dsym_command COMMAND dsymutil $<TARGET_FILE:${target}> -o $<TARGET_FILE:${target}>.dSYM)
    endif()

    add_custom_command(TARGET ${target} POST_BUILD
        ${create_dsym_command}
        COMMAND ${CMAKE_STRIP} -S $<TARGET_FILE:${target}>
        COMMENT "Stripping ${target}")
endfunction()
