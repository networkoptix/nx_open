## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

function(_generate_moc_parameters target parameters_file)
    cmake_parse_arguments(MOC "" "" "INCLUDE_DIRS" ${ARGN})

    set(flags)
    foreach(dir ${MOC_INCLUDE_DIRS})
        if(dir MATCHES "\\.framework/?$")
            string(REGEX REPLACE "/[^/]+\\.framework" "" framework_path ${dir})
            string(APPEND flags "-F${framework_path}\n")
        else()
            string(APPEND flags "-I${dir}\n")
        endif()
    endforeach()

    set(definitions
        "$<TARGET_PROPERTY:${target},COMPILE_DEFINITIONS>")
    set(definitions
        "$<$<BOOL:${definitions}>:-D$<JOIN:${definitions},\n-D>\n>")

    set(options "--no-notes")

    file(GENERATE OUTPUT "${parameters_file}" CONTENT
        "${flags}${definitions}${options}\n")
endfunction()

function(_get_moc_file_name var src_file)
    get_filename_component(full_name ${src_file} ABSOLUTE)
    file(RELATIVE_PATH rel_name ${CMAKE_CURRENT_SOURCE_DIR} ${src_file})
    string(REPLACE ".." "__" rel_moc_name ${rel_name})

    get_filename_component(name ${rel_moc_name} NAME_WE)
    get_filename_component(ext ${rel_moc_name} EXT)
    get_filename_component(path ${rel_moc_name} PATH)

    if(ext STREQUAL ".cpp")
        set(${var} ${CMAKE_CURRENT_BINARY_DIR}/${path}/${name}.moc PARENT_SCOPE)
        return()
    endif()

    if(NOT ext STREQUAL ".h" AND NOT ext STREQUAL ".hpp")
        string(REPLACE "." "_" suffix ${ext})
    else()
        set(suffix)
    endif()

    set(${var} ${CMAKE_CURRENT_BINARY_DIR}/${path}/moc_${name}${suffix}.cpp PARENT_SCOPE)
endfunction()

function(nx_add_qt_mocables target)
    cmake_parse_arguments(MOC "" "" "INCLUDE_DIRS;FLAGS" ${ARGN})

    if(NOT MOC_UNPARSED_ARGUMENTS)
        return()
    endif()

    if(NOT Qt5Core_MOC_EXECUTABLE)
        message(FATAL_ERROR "Qt5 moc is not known: Qt5Core_MOC_EXECUTABLE is not set.")
    endif()

    set(moc_parameters_file
        ${CMAKE_CURRENT_BINARY_DIR}/${target}.moc_parameters$<$<BOOL:$<CONFIGURATION>>:_$<CONFIGURATION>>)
    _generate_moc_parameters(${target} ${moc_parameters_file}
        INCLUDE_DIRS ${MOC_INCLUDE_DIRS})

    set(moc_dirs)

    get_target_property(target_dir ${target} SOURCE_DIR)

    foreach(file ${MOC_UNPARSED_ARGUMENTS})
        if(NOT IS_ABSOLUTE ${file})
            set(file ${target_dir}/${file})
        endif()

        _get_moc_file_name(moc_file ${file})
        get_filename_component(path ${moc_file} PATH)
        list(APPEND moc_dirs ${path})

        add_custom_command(
            OUTPUT ${moc_file}
            COMMAND ${Qt5Core_MOC_EXECUTABLE}
                -o ${moc_file} ${file} @${moc_parameters_file} ${MOC_FLAGS}
            DEPENDS ${file} ${moc_parameters_file}
            VERBATIM
        )

        set_source_files_properties(${file} ${moc_file} PROPERTIES SKIP_AUTOMOC TRUE)
        set_property(TARGET ${target} APPEND PROPERTY SOURCES ${moc_file})

        if(moc_file MATCHES ".moc$")
            get_filename_component(path ${moc_file} PATH)
            set_property(SOURCE ${file} APPEND_STRING PROPERTY COMPILE_FLAGS " -I${path}")
        endif()
    endforeach()

    list(REMOVE_DUPLICATES moc_dirs)
    foreach(dir IN LISTS moc_dirs)
        file(MAKE_DIRECTORY ${dir})
    endforeach()
endfunction()
