function(nx_generate_qrc qrc_file)
    message(STATUS "Generating ${qrc_file}")

    set(content
        "<!DOCTYPE RCC>\n"
        "<RCC version=\"1.0\">\n"
        "<qresource prefix=\"/\">\n")

    set(inputs ${ARGN})
    list(REVERSE inputs)
    set(aliases)

    foreach(input ${inputs})
        if(IS_DIRECTORY ${input})
            file(GLOB_RECURSE files "${input}/*")
            foreach(file ${files})
                file(RELATIVE_PATH alias ${input} ${file})
                list(FIND aliases ${alias} index)
                if(index EQUAL -1)
                    list(APPEND aliases ${alias})
                    list(APPEND content "    <file alias=\"${alias}\">${file}</file>\n")
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
            endif()
        endif()
    endforeach()

    list(APPEND content
        "</qresource>\n"
        "</RCC>\n")

    file(WRITE ${qrc_file}.copy ${content})
    configure_file(${qrc_file}.copy ${qrc_file} COPYONLY)
endfunction()
