include(CMakeParseArguments)

if(CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_MULTI_CONFIGURATION_MODE TRUE)

    if(CMAKE_BUILD_TYPE MATCHES "Default|Debug")
        set(CMAKE_ACTIVE_CONFIGURATIONS "Debug" "Release")
    else()
        set(CMAKE_ACTIVE_CONFIGURATIONS ${CMAKE_BUILD_TYPE})
    endif()
else()
    set(CMAKE_MULTI_CONFIGURATION_MODE FALSE)
    set(CMAKE_ACTIVE_CONFIGURATIONS)
endif()

macro(nx_expose_to_parent_scope variable)
    set(${variable} ${${variable}} PARENT_SCOPE)
endmacro()

macro(nx_expose_variables_to_parent_scope variables_list)
    foreach(variable IN LISTS ${variables_list})
        nx_expose_to_parent_scope(${variable})
    endforeach()
endmacro()

function(nx_set_variable_if_empty variable value)
    if("${${variable}}" STREQUAL "")
        set(${variable} ${value} PARENT_SCOPE)
    endif()
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

        set(need_copy FALSE)

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

function(nx_copy_package_for_configuration package_dir config)
    cmake_parse_arguments(COPY "SKIP_BIN;SKIP_LIB" "" "" ${ARGN})
    if(config)
        string(TOUPPER ${config} CONFIG)
    endif()

    if(NOT COPY_SKIP_BIN)
        if(IS_DIRECTORY "${package_dir}/bin")
            file(GLOB entries "${package_dir}/bin/*")
            foreach(entry ${entries})
                if(CONFIG)
                    nx_copy_package_entry("${entry}" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY_${CONFIG}}")
                else()
                    nx_copy_package_entry("${entry}" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
                endif()
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
                    if(CONFIG)
                        nx_copy_package_entry("${entry}"
                            "${CMAKE_LIBRARY_OUTPUT_DIRECTORY_${CONFIG}}")
                    else()
                        nx_copy_package_entry("${entry}" "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}")
                    endif()
                endforeach()
            endif()
        endif()
    endif()
endfunction()

function(nx_copy_package package_dir)
    cmake_parse_arguments(COPY "" "" "CONFIGURATIONS" ${ARGN})

    if(NOT COPY_CONFIGURATIONS)
        set(COPY_CONFIGURATIONS ${CMAKE_ACTIVE_CONFIGURATIONS})
    endif()

    if(COPY_CONFIGURATIONS)
        foreach(config ${COPY_CONFIGURATIONS})
            nx_copy_package_for_configuration("${package_dir}" "${config}"
                ${COPY_UNPARSED_ARGUMENTS})
        endforeach()
    else()
        nx_copy_package_for_configuration("${package_dir}" "" ${COPY_UNPARSED_ARGUMENTS})
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

nx_init_known_files_list()
