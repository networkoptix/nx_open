function(_generate_moc_parameters target parameters_file)
    set(include_directories
        "$<TARGET_PROPERTY:${target},INCLUDE_DIRECTORIES>"
        ${CMAKE_CXX_STANDARD_INCLUDE_DIRECTORIES})
    set(include_directories
        "$<$<BOOL:${include_directories}>:-I$<JOIN:${include_directories},\n-I>\n>")

    if(MACOSX)
        set(framework_directories
            "$<$<BOOL:${CMAKE_FRAMEWORK_PATH}>:-F $<JOIN:${CMAKE_FRAMEWORK_PATH},\n-F >\n>")
    else()
        set(framework_directories)
    endif()

    set(definitions
        "$<TARGET_PROPERTY:${target},COMPILE_DEFINITIONS>")
    set(definitions
        "$<$<BOOL:${definitions}>:-D$<JOIN:${definitions},\n-D>\n>")

    set(options "--no-notes\n")

    file(GENERATE OUTPUT "${parameters_file}" CONTENT
        "${include_directories}${framework_directories}${definitions}${options}\n")
endfunction()

function(_get_moc_file_name var src_file)
    get_filename_component(full_name ${src_file} ABSOLUTE)
    file(RELATIVE_PATH rel_name ${CMAKE_CURRENT_SOURCE_DIR} ${src_file})
    string(REPLACE ".." "__" rel_moc_name ${rel_name})

    get_filename_component(name ${rel_moc_name} NAME_WE)
    get_filename_component(ext ${rel_moc_name} EXT)
    get_filename_component(path ${rel_moc_name} PATH)

    if(NOT ext STREQUAL ".h" AND NOT ext STREQUAL ".hpp")
        string(REPLACE "." "_" suffix ${ext})
    else()
        set(suffix)
    endif()

    set(${var} ${CMAKE_CURRENT_BINARY_DIR}/${path}/moc_${name}${suffix}.cpp PARENT_SCOPE)
endfunction()

function(nx_add_qt_mocables target)
    if(NOT Qt5Core_MOC_EXECUTABLE)
        message(FATAL_ERROR "Qt5 moc is not known: Qt5Core_MOC_EXECUTABLE is not set.")
    endif()

    set(moc_parameters_file
        ${CMAKE_CURRENT_BINARY_DIR}/${target}.moc_parameters)
    _generate_moc_parameters(${target} ${moc_parameters_file})

    foreach(file ${ARGN})
        _get_moc_file_name(moc_file ${file})

        add_custom_command(
            OUTPUT ${moc_file}
            COMMAND ${Qt5Core_MOC_EXECUTABLE} -o ${moc_file} ${file} @${moc_parameters_file}
            DEPENDS ${file} ${moc_parameters_file}
            ${_moc_working_dir}
            VERBATIM
        )

        set_source_files_properties(${moc_file} PROPERTIES SKIP_AUTOMOC TRUE)
        set_property(TARGET ${target} APPEND PROPERTY SOURCES ${moc_file})
    endforeach()
endfunction()
