## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

include_guard(GLOBAL)

include(CMakeParseArguments)

set(CMAKE_CONFIGURATION_TYPES ${CMAKE_BUILD_TYPE})

set(open_build_utils_dir "${open_source_root}/build_utils")
set(nx_utils_dir "${open_source_root}/libs/nx_utils")

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

function(nx_store_ignored_build_directory path)
    file(RELATIVE_PATH dir ${CMAKE_BINARY_DIR} "${path}")
    set_property(GLOBAL APPEND_STRING PROPERTY known_files
        "${dir}/\n") #< Trailing slash is important!
endfunction()

function(nx_store_known_files)
    foreach(file_name ${ARGV})
        nx_store_known_file(${file_name})
    endforeach()
endfunction()

function(nx_store_known_files_in_directory directory)
    file(GLOB_RECURSE files CONFIGURE_DEPENDS ${directory}/*)
    nx_store_known_files(${files})
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

        if(CMAKE_SCRIPT_MODE_FILE)
            continue()
        endif()

        if(IS_DIRECTORY ${src})
            file(GLOB_RECURSE files RELATIVE ${src} CONFIGURE_DEPENDS "${src}/*")

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
    file(GLOB_RECURSE files RELATIVE ${input} CONFIGURE_DEPENDS ${input}/*)
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
        file(GLOB_RECURSE files RELATIVE ${src} CONFIGURE_DEPENDS "${src}/*")

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
            file(GLOB entries CONFIGURE_DEPENDS "${package_dir}/bin/*")
            foreach(entry ${entries})
                nx_copy_package_entry("${entry}" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
            endforeach()
        endif()
    endif()

    if(NOT COPY_SKIP_LIB)
        if(NOT WINDOWS)
            if(IS_DIRECTORY "${package_dir}/lib")
                if(MACOSX)
                    file(GLOB entries CONFIGURE_DEPENDS "${package_dir}/lib/*.dylib*")
                else()
                    file(GLOB entries CONFIGURE_DEPENDS "${package_dir}/lib/*.so*")
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
            file(GLOB entries CONFIGURE_DEPENDS "${package_dir}/bin/*")
            foreach(entry ${entries})
                nx_copy_package_entry("${entry}" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${separation_dir_name}")
            endforeach()
        endif()
    endif()

    if(NOT COPY_SKIP_LIB)
        if(NOT WINDOWS)
            if(IS_DIRECTORY "${package_dir}/lib")
                if(MACOSX)
                    file(GLOB entries CONFIGURE_DEPENDS "${package_dir}/lib/*.dylib*")
                else()
                    file(GLOB entries CONFIGURE_DEPENDS "${package_dir}/lib/*.so*")
                endif()
                foreach(entry ${entries})
                    nx_copy_package_entry("${entry}" "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${separation_dir_name}")
                endforeach()
            endif()
        endif()
    endif()
endfunction()

# TODO Extend nx_copy function
function(nx_copy_bin_resources input output)
  file(GLOB files RELATIVE ${input} CONFIGURE_DEPENDS ${input}/*)
  foreach(file ${files})
    message(STATUS "copying ${file}")
    nx_copy("${input}/${file}" DESTINATION "${output}")
  endforeach(file)
endfunction()

function(nx_create_qt_conf file_name)
    cmake_parse_arguments(CONF "" "QT_PREFIX;QT_LIBEXEC" "" ${ARGN})

    if(NOT CONF_QT_PREFIX)
        set(qt_prefix "..")
    else()
        set(qt_prefix ${CONF_QT_PREFIX})
    endif()

    if(NOT CONF_QT_LIBEXEC)
        set(qt_libexec "libexec")
    else()
        set(qt_libexec ${CONF_QT_LIBEXEC})
    endif()

    if(LINUX)
        set(qt_conf_file_name qt.linux.conf.in)
    else()
        set(qt_conf_file_name qt.default.conf.in)
    endif()

    nx_configure_file(${open_source_root}/cmake/${qt_conf_file_name} ${file_name})
    nx_store_known_file(${file_name})
endfunction()

function(nx_find_first_matching_file var glob)
    file(GLOB files CONFIGURE_DEPENDS "${glob}")
    if(files)
        list(GET files 0 file)
    else()
        set(file)
    endif()
    set(${var} ${file} PARENT_SCOPE)
endfunction()

function(nx_json_to_cmake json cmake prefix)
    execute_process(COMMAND ${PYTHON_EXECUTABLE}
        ${open_build_utils_dir}/json_to_cmake.py
        ${json} ${cmake} -p ${prefix})
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

# Enquotes and escapes the string (Unix shell style). ATTENTION: This is a very basic version, use
# with caution.
function(nx_shell_escape_string value out_var)
    set(${out_var} "${value}")
    string(REPLACE "\\" "\\\\" value "${value}")
    string(REPLACE "\"" "\\\"" value "${value}")
    string(REPLACE "\'" "\\\'" value "${value}")
    string(REPLACE "\`" "\\\`" value "${value}")

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

macro(nx_execute_process_or_fail)
    cmake_parse_arguments(_NX_EXEC "" "RESULT_VARIABLE" "ERROR_MESSAGE" ${ARGN})

    execute_process(${_NX_EXEC_UNPARSED_ARGUMENTS} RESULT_VARIABLE _nx_exec_result)

    if(NOT _nx_exec_result STREQUAL "0")
        cmake_parse_arguments(
                _NX_EXEC_PROC_ARGS
                "OUTPUT_QUIET;ERROR_QUIET;OUTPUT_STRIP_TRAILING_WHITESPACE;ERROR_STRIP_TRAILING_WHITESPACE;ECHO_OUTPUT_VARIABLE;ECHO_ERROR_VARIABLE"
                "WORKING_DIRECTORY;TIMEOUT;RESULT_VARIABLE;RESULTS_VARIABLE;OUTPUT_VARIABLE;ERROR_VARIABLE;INPUT_FILE;OUTPUT_FILE;ERROR_FILE;COMMAND_ECHO;ENCODING;COMMAND_ERROR_IS_FATAL"
                "COMMAND"
                ${_NX_EXEC_UNPARSED_ARGUMENTS}
        )
        list(JOIN _NX_EXEC_PROC_ARGS_COMMAND " " _NX_EXEC_COMMAND_STRING)

        if(NOT _NX_EXEC_ERROR_MESSAGE)
            set(_NX_EXEC_ERROR_MESSAGE "Process execution failed.")
        else()
            list(JOIN _NX_EXEC_ERROR_MESSAGE " " _NX_EXEC_ERROR_MESSAGE)
        endif()

        message(
            FATAL_ERROR
            ${_NX_EXEC_ERROR_MESSAGE}
            "\nCommand: \"${_NX_EXEC_COMMAND_STRING}\""
            "\nExit status or error message: \"${_nx_exec_result}\""
        )
    endif()

    set(${_NX_EXEC_RESULT_VARIABLE} ${_nx_exec_result})
endmacro()

function(nx_add_merge_directories_target target)
    cmake_parse_arguments(MERGE "" "OUTPUT" "DEPENDS" ${ARGN})

    set(rules)
    set(depends)

    # Parse rules and fill dependencies.
    set(waiting_for_prefix OFF)
    set(current_prefix)

    foreach(arg ${MERGE_UNPARSED_ARGUMENTS})
        if(waiting_for_prefix)
            set(current_prefix ${arg})
            set(waiting_for_prefix OFF)
            continue()
        elseif(arg STREQUAL "PREFIX")
            set(waiting_for_prefix ON)
            continue()
        endif()

        # Gather dependencies list.
        if(EXISTS ${arg})
            if(IS_DIRECTORY ${arg})
                file(GLOB_RECURSE files CONFIGURE_DEPENDS ${arg}/*)
                list(APPEND depends ${files})
            else()
                list(APPEND depends ${arg})
            endif()
        endif()

        list(APPEND rules ${current_prefix}:${arg})
    endforeach()

    add_custom_target(${target}
        COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_SOURCE_DIR}/build_utils/merge_and_sync_directories.py
            ${rules} ${MERGE_OUTPUT}
        DEPENDS ${MERGE_DEPENDS} ${depends}
    )
endfunction()

function(nx_subtract_lists minuend subtrahend)
    foreach(item ${${subtrahend}})
        list(REMOVE_ITEM ${minuend} ${item})
    endforeach()
    nx_expose_to_parent_scope(${minuend})
endfunction()

function(nx_remove_proprietary_docs input output)
    execute_process(COMMAND
        ${PYTHON_EXECUTABLE} ${open_build_utils_dir}/remove_proprietary_docs.py
        ${input} ${output})

    nx_store_known_file(${output})
endfunction()

function(nx_check_package_paths_changes package_type)
    foreach(package ${ARGN})
        string(TOUPPER "${package_type}_${package}_ROOT" package_path_variable_name)
        string(TOUPPER "PACKAGE_${package}_ROOT" cached_path_variable_name)

        if(NOT DEFINED ${cached_path_variable_name})
            set(${cached_path_variable_name} "${${package_path_variable_name}}"
                CACHE INTERNAL "Path to package ${package}."
            )
        elseif(NOT "${${cached_path_variable_name}}" STREQUAL "${${package_path_variable_name}}")
            if(NOT ${package} IN_LIST cache_spoiling_packages)
                continue()
            endif()

            message(FATAL_ERROR
                "\nPath to package ${package} has been changed from "
                "\"${${cached_path_variable_name}}\" to \"${${package_path_variable_name}}\".\n"
                "You should manually remove CMakeCache.txt from the build directory.\n"
                "ATTENTION: This will lead to reseting all the custom build parameters that could "
                "have been added by you and you will have to restore them manually.\n"
            )
        endif()
    endforeach()
endfunction()

# Check if we need a newer libstdc++.
function(nx_detect_default_use_system_stdcpp_value developer_build)
    if(NOT DEFINED CACHE{useSystemStdcpp})
        set(useSystemStdcpp OFF)

        if(developer_build AND arch MATCHES "x64|x86")
            # CMAKE_CXX_FLAGS could contain flags which affect search directories. For example
            # our Linux toolchain file puts `--gcc-toolchain` parameter there when useClang
            # option is set.
            separate_arguments(flags UNIX_COMMAND ${CMAKE_CXX_FLAGS})
            execute_process(
                COMMAND ${CMAKE_CXX_COMPILER} ${flags} --print-file-name libstdc++.so.6
                OUTPUT_VARIABLE stdcpp_lib_path
            )

            if(stdcpp_lib_path)
                get_filename_component(stdcpp_dir ${stdcpp_lib_path} DIRECTORY)
                execute_process(
                    COMMAND ${open_build_utils_dir}/linux/choose_newer_stdcpp.sh
                        ${stdcpp_dir}
                    OUTPUT_VARIABLE selected_stdcpp
                )
            else()
                set(selected_stdcpp)
            endif()

            if(NOT selected_stdcpp)
                set(useSystemStdcpp ON)
            endif()
        endif()

        set(useSystemStdcpp ${useSystemStdcpp} CACHE BOOL
            "Use system libstdc++ and do not copy it to lib directory from the compiler.")
    endif()
endfunction()

function(nx_copy_system_libraries)
    execute_process(COMMAND
        ${PYTHON_EXECUTABLE} ${open_build_utils_dir}/linux/copy_system_library.py
            --compiler=${CMAKE_CXX_COMPILER}
            --flags=${CMAKE_CXX_FLAGS}
            --link-flags=${CMAKE_SHARED_LINKER_FLAGS}
            --dest-dir=${CMAKE_LIBRARY_OUTPUT_DIRECTORY}
            --list
            ${ARGN}
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
    )

    if(NOT result EQUAL 0)
        message(FATAL_ERROR "Cannot copy system libraries: ${ARGN}.")
    endif()

    nx_split_string(files "${output}")
    nx_store_known_files(${files})
endfunction()

function(nx_add_custom_command)
    cmake_parse_arguments(ARGS "" "" "BYPRODUCTS;OUTPUT" ${ARGN})

    if(NOT ARGS_BYPRODUCTS)
        message(FATAL_ERROR "Missing mandatory parameter BYPRODUCTS")
    endif()
    if(NOT ARGS_OUTPUT)
        message(FATAL_ERROR "Missing mandatory parameter OUTPUT")
    endif()

    set(add_custom_command_args ${ARGS_UNPARSED_ARGUMENTS})
    list(PREPEND add_custom_command_args OUTPUT ${ARGS_OUTPUT})
    list(PREPEND add_custom_command_args
        COMMAND ${CMAKE_COMMAND} -E remove_directory ${ARGS_BYPRODUCTS}
    )

    list(APPEND add_custom_command_args
        BYPRODUCTS ${ARGS_BYPRODUCTS}
        COMMAND ${PYTHON_EXECUTABLE} "${open_build_utils_dir}/ninja/ninja_tool.py"
            -d "${CMAKE_BINARY_DIR}"
            -e add_directories_to_known_files "${ARGS_BYPRODUCTS}"
    )

    add_custom_command(${add_custom_command_args})
endfunction()

# Recursively gathers all MANUALLY_ADDED_DEPENDENCIES for a target.
function(nx_get_target_manual_dependencies target out_deps)
    get_target_property(deps ${target} MANUALLY_ADDED_DEPENDENCIES)

    if(NOT deps)
        set(${out_deps} PARENT_SCOPE)
        return()
    endif()

    set(local_out_deps)
    foreach(dep IN LISTS deps)
        if(TARGET ${dep})
            nx_get_target_manual_dependencies(${dep} local_out_deps2)
            list(APPEND local_out_deps ${local_out_deps2})
        endif()
    endforeach()

    set(${out_deps} ${deps} ${local_out_deps} PARENT_SCOPE)
endfunction()
