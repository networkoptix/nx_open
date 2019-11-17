find_program(heat_executable heat HINTS ${wix_directory}/bin NO_DEFAULT_PATH)
if(NOT heat_executable)
    message(FATAL_ERROR "Cannot find heat.")
endif()

find_program(candle_executable candle HINTS ${wix_directory}/bin NO_DEFAULT_PATH)
if(NOT candle_executable)
    message(FATAL_ERROR "Cannot find candle.")
endif()

function(nx_wix_heat target)
    set(oneValueArgs
        SOURCE_DIR
        TARGET_DIR_ALIAS
        COMPONENT_GROUP)

    cmake_parse_arguments(WXS "" "${oneValueArgs}" "" ${ARGN})

    set(wxs_file ${CMAKE_CURRENT_BINARY_DIR}/${WXS_COMPONENT_GROUP}.wxs)
    set(wixobj_file ${CMAKE_CURRENT_BINARY_DIR}/${WXS_COMPONENT_GROUP}.wixobj)
    set(source_dir_var_name "${WXS_COMPONENT_GROUP}_dir")
    set(source_dir_alias "var.${source_dir_var_name}")

    execute_process(
        COMMAND ${heat_executable}
            dir ${WXS_SOURCE_DIR}
            -out ${wxs_file}.copy
            -var ${source_dir_alias}
            -dr ${WXS_TARGET_DIR_ALIAS}
            -cg ${WXS_COMPONENT_GROUP}
            -ag
            -srd
            -sfrag
            -sreg
            -suid
            -nologo
        )

    file(TIMESTAMP "${wxs_file}" orig_ts)
    execute_process(
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${wxs_file}.copy ${wxs_file}
        RESULT_VARIABLE result)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "Cannot create ${wxs_file}")
    endif()
    file(REMOVE ${wxs_file}.copy)
    file(TIMESTAMP "${wxs_file}" new_ts)

    if(NOT ${orig_ts} STREQUAL ${new_ts})
        message(STATUS "Generated ${wxs_file}")
    endif()

    add_custom_command(
        COMMENT "[Wix] Candle ${WXS_COMPONENT_GROUP}"
        MAIN_DEPENDENCY ${wxs_file}
        COMMAND ${candle_executable}
            -arch x64
            -nologo
            -d${source_dir_var_name}=${WXS_SOURCE_DIR}
            -out ${wixobj_file}
            ${wxs_file}
        OUTPUT ${wixobj_file}
        VERBATIM
    )

    add_custom_target(${target}
        DEPENDS
            ${wixobj_file}
        COMMENT "[Wix] Generating ${WXS_COMPONENT_GROUP}"
    )
    set_target_properties(${target} PROPERTIES FOLDER distribution)

endfunction()
