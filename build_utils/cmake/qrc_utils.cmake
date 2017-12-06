function(nx_generate_qrc qrc_file)
    cmake_parse_arguments(QRC "" "" "USED_FILES_VARIABLE" ${ARGN})

    message(STATUS "Generating ${qrc_file}")

    set(content
        "<!DOCTYPE RCC>\n"
        "<RCC version=\"1.0\">\n"
        "<qresource prefix=\"/\">\n")

    set(inputs ${ARGN})
    list(REVERSE inputs)
    set(aliases)
    set(used_files)

    foreach(input ${inputs})
        if(IS_DIRECTORY ${input})
            file(GLOB_RECURSE files "${input}/*")
            foreach(file ${files})
                file(RELATIVE_PATH alias ${input} ${file})
                list(FIND aliases ${alias} index)
                if(index EQUAL -1)
                    list(APPEND aliases ${alias})
                    list(APPEND content "    <file alias=\"${alias}\">${file}</file>\n")
                    list(APPEND used_files ${file})
                endif()
            endforeach()
        else()
            get_source_file_property(base ${input} RESOURCE_BASE_DIR)
            if(base)
                file(RELATIVE_PATH alias ${base} ${input})
            else()
                get_filename_component(alias ${input} NAME)
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
    execute_process(
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${qrc_file}.copy ${qrc_file}
        RESULT_VARIABLE result)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "Cannot create ${qrc_file}")
    endif()

    if(QRC_USED_FILES_VARIABLE)
        set(${QRC_USED_FILES_VARIABLE} ${used_files} PARENT_SCOPE)
    endif()
endfunction()

function(nx_add_qrc qrc_file out_cpp_file_variable)
    cmake_parse_arguments(QRC "" "" "OPTIONS;DEPENDS" ${ARGN})

    if(NOT Qt5Core_RCC_EXECUTABLE)
        message(FATAL_ERROR
            "rcc executable is not known: variable Qt5Core_RCC_EXECUTABLE is not set. "
            "Find Qt5Core module before adding a qrc file.")
    endif()

    get_filename_component(resource_name ${qrc_file} NAME_WE)
    get_filename_component(qrc_file ${qrc_file} ABSOLUTE)
    set(cpp_file ${CMAKE_CURRENT_BINARY_DIR}/qrc_${resource_name}.cpp)

    add_custom_command(
        OUTPUT ${cpp_file}
        COMMAND ${Qt5Core_RCC_EXECUTABLE}
        ARGS ${QRC_OPTIONS} --name ${resource_name} --output ${cpp_file} ${qrc_file}
        MAIN_DEPENDENCY ${qrc_file}
        DEPENDS ${QRC_DEPENDS}
        VERBATIM
    )

    if(out_cpp_file_variable)
        set(${out_cpp_file_variable} ${cpp_file} PARENT_SCOPE)
    endif()
endfunction()
