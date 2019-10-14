find_program(heat_executable heat HINTS ${wix_directory}/bin NO_DEFAULT_PATH)
if(NOT heat_executable)
    message(FATAL_ERROR "Cannot find heat.")
endif()

find_program(candle_executable candle HINTS ${wix_directory}/bin NO_DEFAULT_PATH)
if(NOT candle_executable)
    message(FATAL_ERROR "Cannot find candle.")
endif()

function(nx_generate_wxs wxs_file)
    set(oneValueArgs
        SOURCE_DIR
        TARGET_DIR_ALIAS
        COMPONENT_GROUP)

    cmake_parse_arguments(WXS "" "${oneValueArgs}" "" ${ARGN})

    set(content
        "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
        "<Wix xmlns=\"http://schemas.microsoft.com/wix/2006/wi\">\n"
        "    <Fragment>\n"
        "        <ComponentGroup\n"
        "            Id=\"${WXS_COMPONENT_GROUP}\"\n"
        "            Source=\"${WXS_SOURCE_DIR}\"\n"
        "            Directory=\"${WXS_TARGET_DIR_ALIAS}\">\n")

    set(inputs ${WXS_UNPARSED_ARGUMENTS})
    #list(REVERSE inputs)

    file(GLOB_RECURSE files "${WXS_SOURCE_DIR}/*")
    foreach(file ${files})
        file(RELATIVE_PATH alias ${WXS_SOURCE_DIR} ${file})
        list(APPEND content
            "                <Component><File Name=\"${alias}\" KeyPath=\"yes\"/></Component>\n")
    endforeach()

    foreach(input ${inputs})
        if(IS_DIRECTORY ${input})
            file(GLOB_RECURSE files "${input}/*")
            foreach(file ${files})
                file(RELATIVE_PATH alias ${WXS_SOURCE_DIR} ${file})
                list(APPEND content
                    "                <Component><File Name=\"${alias}\" KeyPath=\"yes\"/></Component>\n")
            endforeach()
        else()
            file(RELATIVE_PATH alias ${WXS_SOURCE_DIR} ${input})
            list(APPEND content
                    "                <Component><File Name=\"${alias}\" KeyPath=\"yes\"/></Component>\n")
        endif()
    endforeach()

    list(APPEND content
        "        </ComponentGroup>\n"
        "    </Fragment>\n"
        "</Wix>\n")

    file(WRITE ${wxs_file}.copy ${content})

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
endfunction()

function(nx_wix_heat wxs_file)
    set(oneValueArgs
        SOURCE_DIR
        SOURCE_DIR_ALIAS
        TARGET_DIR_ALIAS
        COMPONENT_GROUP)

    cmake_parse_arguments(WXS "" "${oneValueArgs}" "" ${ARGN})

    execute_process(
        COMMAND ${heat_executable}
            dir ${WXS_SOURCE_DIR}
            -out ${wxs_file}.copy
            -var ${WXS_SOURCE_DIR_ALIAS}
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
endfunction()
