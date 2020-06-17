include(CMakeParseArguments)

set(CMAKE_CONFIGURATION_TYPES ${CMAKE_BUILD_TYPE})

macro(nx_expose_to_parent_scope variable)
    set(${variable} ${${variable}} PARENT_SCOPE)
endmacro()

macro(nx_expose_variables_to_parent_scope)
    foreach(variable ${ARGN})
        nx_expose_to_parent_scope(${variable})
    endforeach()
endmacro()

function(nx_set_variable_if_empty variable value)
    if("${${variable}}" STREQUAL "")
        set(${variable} ${value} PARENT_SCOPE)
    endif()
endfunction()

function(nx_split_string var string)
    cmake_parse_arguments(SPLIT "" "SEPARATOR" "" ${ARGN})

    if(NOT SPLIT_SEPARATOR)
        set(SPLIT_SEPARATOR "[\n| |\t]+")
    endif()

    string(REGEX REPLACE "${SPLIT_SEPARATOR}" ";" result "${string}")
    set(${var} ${result} PARENT_SCOPE)
endfunction()

function(nx_init_known_files_list)
    get_property(property_set GLOBAL PROPERTY known_files SET)

    if(NOT property_set)
        set_property(GLOBAL PROPERTY known_files "known_files.txt\n")
    endif()
endfunction()

function(nx_store_known_file file_name)
    file(RELATIVE_PATH file ${CMAKE_BINARY_DIR} "${file_name}")
    set_property(GLOBAL APPEND_STRING PROPERTY known_files "${file}\n")
endfunction()

function(nx_store_known_files)
    foreach(file_name ${ARGV})
        nx_store_known_file(${file_name})
    endforeach()
endfunction()

function(nx_save_known_files)
    get_property(files GLOBAL PROPERTY known_files)
    file(WRITE ${CMAKE_BINARY_DIR}/known_files.txt ${files})
endfunction()

function(nx_get_copy_full_destination_name var src dst)
    if(IS_DIRECTORY ${dst})
        get_filename_component(basename ${src} NAME)
        set(${var} ${dst}/${basename} PARENT_SCOPE)
    else()
        set(${var} ${dst} PARENT_SCOPE)
    endif()
endfunction()

function(nx_copy)
    cmake_parse_arguments(COPY "IF_NEWER;IF_DIFFERENT;IF_MISSING" "DESTINATION" "" ${ARGN})

    if(NOT COPY_DESTINATION)
        message(FATAL_ERROR "DESTINATION must be provided.")
    endif()

    if(NOT EXISTS "${COPY_DESTINATION}")
        file(MAKE_DIRECTORY "${COPY_DESTINATION}")
    elseif(NOT IS_DIRECTORY "${COPY_DESTINATION}")
        message(FATAL_ERROR "DESTINATION must be a directory")
    endif()

    foreach(src ${COPY_UNPARSED_ARGUMENTS})
        nx_get_copy_full_destination_name(dst ${src} ${COPY_DESTINATION})

        if(COPY_IF_NEWER)
            file(TIMESTAMP "${src}" src_ts)
            file(TIMESTAMP "${dst}" dst_ts)
            if(NOT "${src_ts}" STREQUAL "${dst_ts}")
                message(STATUS "Copying ${src} to ${dst}")
                file(COPY "${src}" DESTINATION "${COPY_DESTINATION}")
            endif()
        elseif(COPY_IF_DIFFERENT)
            message(STATUS "Copying ${src} to ${dst}")
            execute_process(COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${src}" "${dst}")
        elseif(COPY_IF_MISSING)
            if(NOT EXISTS "${dst}")
                message(STATUS "Copying ${src} to ${dst}")
                file(COPY "${src}" DESTINATION "${COPY_DESTINATION}")
            endif()
        else()
            message(STATUS "Copying ${src} to ${dst}")
            # CMake skips copying if source and destination timestamps are equal. We have to remove
            # the existing destination file to ensure the file is updated.
            file(REMOVE ${dst})
            file(COPY "${src}" DESTINATION "${COPY_DESTINATION}")
        endif()

        if(IS_DIRECTORY ${src})
            file(GLOB_RECURSE files RELATIVE ${src} "${src}/*")

            foreach(file ${files})
                nx_store_known_file(${dst}/${file})
            endforeach()
        else()
            nx_store_known_file(${dst})
        endif()
    endforeach()
endfunction()

function(nx_configure_file input output)
    if(IS_DIRECTORY ${output})
        get_filename_component(file_name ${input} NAME)
        set(output "${output}/${file_name}")
    elseif(output MATCHES "^.+/$")
        get_filename_component(file_name ${input} NAME)
        set(output "${output}${file_name}")
    endif()

    file(TIMESTAMP "${output}" orig_ts)
    
    # TODO: Add `NEWLINE_STYLE UNIX`, because on Windows configure_file() produces CRLF by default.
    # Then make sure all generated files work correctly, especially MSVC files, .xml and .wxi.
    configure_file(${input} ${output} ${ARGN})
    
    file(TIMESTAMP "${output}" new_ts)

    nx_store_known_file(${output})

    if(NOT ${orig_ts} STREQUAL ${new_ts})
        message(STATUS "Generated ${output}")
    endif()
endfunction()

function(nx_configure_directory input output)
    cmake_parse_arguments(ARG "" "OUTPUT_FILES_VARIABLE" "" ${ARGN})

    set(output_files)

    message(STATUS "Configuring directory (recursively) ${input}")
    file(GLOB_RECURSE files RELATIVE ${input} ${input}/*)
    foreach(file ${files})
        set(src_template_path ${input}/${file})
        if(NOT IS_DIRECTORY ${src_template_path})
            nx_configure_file(${input}/${file} ${output}/${file} ${ARG_UNPARSED_ARGUMENTS})
            list(APPEND output_files ${output}/${file})
        endif()
    endforeach()

    if(ARG_OUTPUT_FILES_VARIABLE)
        set(${ARG_OUTPUT_FILES_VARIABLE} ${output_files} PARENT_SCOPE)
    endif()
endfunction()

# Unconditinally copy files to target_dir, reproducing parts of their path relative to source_dir.
function(nx_copy_always source_dir source_absolute_files target_dir)
    foreach(file ${source_absolute_files})
        file(RELATIVE_PATH relative_file ${source_dir} ${file})
        get_filename_component(relative_dir ${relative_file} DIRECTORY)
        file(COPY ${file} DESTINATION ${target_dir}/${relative_dir})
    endforeach()
endfunction()

function(nx_copy_package_entry src dst)
    if(NOT EXISTS ${src})
        message(FATAL_ERROR "${src} does not exist.")
    endif()

    file(COPY ${src} DESTINATION ${dst})

    nx_get_copy_full_destination_name(full_dst ${src} ${dst})

    if(IS_DIRECTORY ${src})
        file(GLOB_RECURSE files RELATIVE ${src} "${src}/*")

        foreach(file ${files})
            nx_store_known_file(${full_dst}/${file})
        endforeach()
    else()
        nx_store_known_file(${full_dst})
    endif()
endfunction()

function(nx_copy_package package_dir)
    cmake_parse_arguments(COPY "SKIP_BIN;SKIP_LIB" "" "" ${ARGN})

    if(NOT COPY_SKIP_BIN)
        if(IS_DIRECTORY "${package_dir}/bin")
            file(GLOB entries "${package_dir}/bin/*")
            foreach(entry ${entries})
                nx_copy_package_entry("${entry}" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
            endforeach()
        endif()
    endif()

    if(NOT COPY_SKIP_LIB)
        if(NOT WINDOWS)
            if(IS_DIRECTORY "${package_dir}/lib")
                if(MACOSX)
                    file(GLOB entries "${package_dir}/lib/*.dylib*")
                else()
                    file(GLOB entries "${package_dir}/lib/*.so*")
                endif()
                foreach(entry ${entries})
                    nx_copy_package_entry("${entry}" "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}")
                endforeach()
            endif()
        endif()
    endif()
endfunction()

function(nx_copy_package_separately package_dir separation_dir_name)
    cmake_parse_arguments(COPY "SKIP_BIN;SKIP_LIB" "" "" ${ARGN})

    if(NOT COPY_SKIP_BIN)
        if(IS_DIRECTORY "${package_dir}/bin")
            file(GLOB entries "${package_dir}/bin/*")
            foreach(entry ${entries})
                nx_copy_package_entry("${entry}" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${separation_dir_name}")
            endforeach()
        endif()
    endif()

    if(NOT COPY_SKIP_LIB)
        if(NOT WINDOWS)
            if(IS_DIRECTORY "${package_dir}/lib")
                if(MACOSX)
                    file(GLOB entries "${package_dir}/lib/*.dylib*")
                else()
                    file(GLOB entries "${package_dir}/lib/*.so*")
                endif()
                foreach(entry ${entries})
                    nx_copy_package_entry("${entry}" "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${separation_dir_name}")
                endforeach()
            endif()
        endif()
    endif()
endfunction()


function(nx_copy_current_package)
    file(GLOB entries ${CMAKE_CURRENT_LIST_DIR}/*)
    foreach(entry ${entries})
        if(NOT entry MATCHES "\\.rdpack$|.cmake$")
            nx_copy_package_entry(${entry} ${CMAKE_BINARY_DIR})
        endif()
    endforeach()
endfunction()

# TODO Extend nx_copy function
function(nx_copy_bin_resources input output)
  file(GLOB files RELATIVE ${input} ${input}/*)
  foreach(file ${files})
    message(STATUS "copying ${file}")
    nx_copy("${input}/${file}" DESTINATION "${output}")
  endforeach(file)
endfunction()

function(nx_create_qt_conf file_name)
    cmake_parse_arguments(CONF "" "QT_PREFIX" "" ${ARGN})

    if(NOT CONF_QT_PREFIX)
        set(qt_prefix "..")
    else()
        set(qt_prefix ${CONF_QT_PREFIX})
    endif()

    nx_configure_file(${CMAKE_SOURCE_DIR}/cmake/qt.conf ${file_name})
    nx_store_known_file(${file_name})
endfunction()

function(nx_find_first_matching_file var glob)
    file(GLOB files "${glob}")
    if(files)
        list(GET files 0 file)
    else()
        set(file)
    endif()
    set(${var} ${file} PARENT_SCOPE)
endfunction()

function(nx_json_to_cmake json cmake prefix)
    if(${json} IS_NEWER_THAN ${cmake})
        execute_process(COMMAND ${PYTHON_EXECUTABLE}
            ${build_utils_dir}/json_to_cmake.py
            ${json} ${cmake} -p ${prefix})
    endif()
endfunction()

# Intended for debug. Enquotes and escapes the string (C style) to the best of CMake feasibility. 
function(nx_c_escape_string value out_var)
    set(${out_var} "${value}")
    string(REPLACE "\\" "\\\\" value "${value}")
    string(REPLACE "\"" "\\\"" value "${value}")
    string(REPLACE "\t" "\\t" value "${value}")
    string(REPLACE "\r" "\\r" value "${value}")
    string(REPLACE "\n" "\\n" value "${value}")
    # There seems to be no way in CMake to escape other control chars, thus leaving them as is.
    
    set(${out_var} "\"${value}\"" PARENT_SCOPE)
endfunction()

# Intended for debug. Prints all arguments as strings.
function(nx_print_args)
    set(output "####### nx_print_args(): ${ARGC} argument(s) (using C escaping):\n")
    string(APPEND output "{\n")
    if(${ARGC}) #< If ARGC is not zero.
        math(EXPR last_arg_index "${ARGC} - 1")
        foreach(arg_index RANGE 0 ${last_arg_index})
            nx_c_escape_string("${ARGV${arg_index}}" c_escaped_arg)
            string(APPEND output "    ${c_escaped_arg}\n")
        endforeach()
    endif()
    string(APPEND output "}")
    
    message("${output}")
endfunction()

# Intended for debug. Prints the variable contents both as a string and as a list.
function(nx_print_var some_var)
    if(NOT DEFINED ${some_var})
        message("####### nx_print_var(${some_var}): not defined")
        return()
    endif()

    list(LENGTH ${some_var} list_length)
    if(NOT DEFINED list_length OR NOT "${list_length}" MATCHES "^[0-9]+$")
        message(FATAL_ERROR "INTERNAL ERROR: list(LENGTH) failed for ${some_var}.")
    endif()
    
    string(LENGTH "${${some_var}}" string_length)
    if(NOT DEFINED string_length OR NOT "${string_length}" MATCHES "^[0-9]+$")
        message(FATAL_ERROR "INTERNAL ERROR: string(LENGTH) failed for ${some_var}.")
        return()
    endif()

    if(list_length EQUAL 0 AND string_length EQUAL 0 AND "${${some_var}}" STREQUAL "")
        message("####### nx_print_var(${some_var}): empty")
        return()
    endif()

    set(output "####### nx_print_var(${some_var}) (using C escaping):\n")
    nx_c_escape_string("${${some_var}}" c_escaped_value)
    string(APPEND output "    As string of length ${string_length}: ${c_escaped_value}\n")
    string(APPEND output "    As list of length ${list_length}:\n")
    string(APPEND output "    {\n")
    if(list_length GREATER 0)
        math(EXPR last_list_item_index "${list_length} - 1")
        foreach(list_item_index RANGE 0 ${last_list_item_index})
            list(GET ${some_var} ${list_item_index} list_item)
            nx_c_escape_string("${list_item}" c_escaped_list_item)
            string(APPEND output "        ${c_escaped_list_item}\n")
        endforeach()
    endif()
    string(APPEND output "    }\n")
    
    message("${output}")
endfunction()
