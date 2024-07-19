## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

function(_nx_validate_rcc_executable)
    get_target_property(QT_RCC_EXECUTABLE Qt6::rcc LOCATION)
    if(NOT QT_RCC_EXECUTABLE)
        message(FATAL_ERROR
            "rcc executable is not known: variable QT_RCC_EXECUTABLE is not set. "
            "Find Qt6Core module before adding a qrc file.")
    endif()
endfunction()

function(nx_set_qrc_source_files_properties)
    cmake_parse_arguments(RESOURCE "" "BASE_DIR;ALIAS_PREFIX" "" ${ARGN})

    set(properties)

    if(RESOURCE_BASE_DIR)
        list(APPEND properties RESOURCE_BASE_DIR ${RESOURCE_BASE_DIR})
    endif()

    if(RESOURCE_ALIAS_PREFIX)
        list(APPEND properties RESOURCE_ALIAS_PREFIX ${RESOURCE_ALIAS_PREFIX})
    endif()

    set_source_files_properties(${RESOURCE_UNPARSED_ARGUMENTS} PROPERTIES ${properties})
endfunction()

function(nx_generate_qrc qrc_file)
    cmake_parse_arguments(QRC "" "" "USED_FILES_VARIABLE" ${ARGN})

    set(content
        "<!DOCTYPE RCC>\n"
        "<RCC version=\"1.0\">\n"
        "<qresource prefix=\"/\">\n")

    set(inputs ${QRC_UNPARSED_ARGUMENTS})
    list(REVERSE inputs)
    set(aliases)
    set(used_files)

    foreach(input ${inputs})
        get_source_file_property(base ${input} RESOURCE_BASE_DIR)
        get_source_file_property(prefix ${input} RESOURCE_ALIAS_PREFIX)
        if(NOT prefix)
            set(prefix) #< Clear NOTFOUND value.
        endif()
        if(IS_DIRECTORY ${input})
            file(GLOB_RECURSE files CONFIGURE_DEPENDS "${input}/*")
            foreach(file ${files})
                if(file MATCHES "(.orig|.rej)$")
                    # These files are often left by version control systems, but they should
                    # definitely not appear in the resources.
                    continue()
                endif()

                if(base)
                    file(RELATIVE_PATH alias ${base} ${file})
                else()
                    file(RELATIVE_PATH alias ${input} ${file})
                endif()
                string(PREPEND alias ${prefix})
                list(FIND aliases ${alias} index)
                if(index EQUAL -1)
                    list(APPEND aliases ${alias})
                    list(APPEND content "    <file alias=\"${alias}\">${file}</file>\n")
                    list(APPEND used_files ${file})
                endif()
            endforeach()
        else()
            get_source_file_property(alias ${input} RESOURCE_ALIAS)
            if(NOT alias)
                if(base)
                    file(RELATIVE_PATH alias ${base} ${input})
                else()
                    get_filename_component(alias ${input} NAME)
                endif()
                string(PREPEND alias ${prefix})
            endif()
            list(FIND aliases ${alias} index)
            if(index EQUAL -1)
                list(APPEND aliases ${alias})
                list(APPEND content "    <file alias=\"${alias}\">${input}</file>\n")
                list(APPEND used_files ${input})
            endif()
        endif()
    endforeach()

    list(APPEND content
        "</qresource>\n"
        "</RCC>\n")

    file(WRITE ${qrc_file}.copy ${content})

    file(TIMESTAMP "${qrc_file}" orig_ts)
    execute_process(
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${qrc_file}.copy ${qrc_file}
        RESULT_VARIABLE result)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "Cannot create ${qrc_file}")
    endif()
    file(REMOVE ${qrc_file}.copy)
    file(TIMESTAMP "${qrc_file}" new_ts)

    if(NOT ${orig_ts} STREQUAL ${new_ts})
        message(STATUS "Generated ${qrc_file}")
    endif()

    if(QRC_USED_FILES_VARIABLE)
        set(${QRC_USED_FILES_VARIABLE} ${used_files} PARENT_SCOPE)
    endif()
endfunction()

function(nx_add_qrc qrc_file out_cpp_file_variable)
    set(multiValueArgs OPTIONS DEPENDS)
    cmake_parse_arguments(QRC "" "" "${multiValueArgs}" ${ARGN})

    get_filename_component(resource_name ${qrc_file} NAME_WE)
    get_filename_component(qrc_file ${qrc_file} ABSOLUTE)

    set(cpp_file ${CMAKE_CURRENT_BINARY_DIR}/qrc_${resource_name}.cpp)

    _nx_validate_rcc_executable()
    get_target_property(QT_RCC_EXECUTABLE Qt6::rcc LOCATION)
    add_custom_command(
        OUTPUT ${cpp_file}
        COMMAND ${QT_RCC_EXECUTABLE}
        ARGS ${QRC_OPTIONS} --name ${resource_name} --output ${cpp_file} ${qrc_file}
        MAIN_DEPENDENCY ${qrc_file}
        DEPENDS ${QRC_DEPENDS}
        VERBATIM
    )

    if(out_cpp_file_variable)
        set(${out_cpp_file_variable} ${cpp_file} PARENT_SCOPE)
    endif()

endfunction()

function(nx_add_external_resources_target target)
    set(oneValueArgs
        QRC_FILE
        TARGET_FILE)
    set(multiValueArgs
        FILES)
    cmake_parse_arguments(QRC "" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    get_filename_component(resource_name ${QRC_TARGET_FILE} NAME_WE)

    set(qrc_file ${CMAKE_CURRENT_BINARY_DIR}/${QRC_QRC_FILE}.qrc)
    nx_generate_qrc(${qrc_file} ${QRC_FILES} USED_FILES_VARIABLE qrc_used_files)

    _nx_validate_rcc_executable()
    get_target_property(QT_RCC_EXECUTABLE Qt6::rcc LOCATION)
    add_custom_command(
        OUTPUT ${QRC_TARGET_FILE}
        COMMAND ${QT_RCC_EXECUTABLE}
            -binary
            --name ${resource_name}
            --output ${QRC_TARGET_FILE}
            ${qrc_file}
        DEPENDS ${qrc_file} ${qrc_used_files}
        VERBATIM
    )

    add_custom_target(${target} ALL
        DEPENDS
            ${QRC_TARGET_FILE}
        COMMENT "[QRC] Generating ${QRC_TARGET_FILE}"
    )
    set_target_properties(${target} PROPERTIES FOLDER resources)
endfunction()
