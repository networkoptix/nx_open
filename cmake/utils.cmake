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

macro(nx_expose_variables_to_parent_scope _variables_list)
    foreach(_variable_name IN LISTS ${_variables_list})
        set(${_variable_name} ${${_variable_name}} PARENT_SCOPE)
    endforeach()
endmacro()

function(nx_set_variable_if_empty variable value)
    if("${${variable}}" STREQUAL "")
        set(${variable} ${value} PARENT_SCOPE)
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

    foreach(file ${COPY_UNPARSED_ARGUMENTS})
        get_filename_component(dst "${file}" NAME)
        set(dst "${COPY_DESTINATION}/${dst}")

        if(COPY_IF_NEWER)
            file(TIMESTAMP "${file}" orig_ts)
            file(TIMESTAMP "${dst}" dst_ts)
            if(NOT "${orig_ts}" STREQUAL "${dst_ts}")
                message(STATUS "Copying ${file} to ${dst}")
                file(COPY "${file}" DESTINATION "${COPY_DESTINATION}")
            endif()
        elseif(COPY_IF_DIFFERENT)
            execute_process(COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${file}" "${dst}")
        elseif(COPY_IF_MISSING)
            if(NOT EXISTS "${dst}")
                file(COPY "${file}" DESTINATION "${COPY_DESTINATION}")
            endif()
        else()
            file(COPY "${file}" DESTINATION "${COPY_DESTINATION}")
        endif()
    endforeach()
endfunction()

function(nx_configure_file input output)
    if(IS_DIRECTORY ${output})
        get_filename_component(file_name ${input} NAME)
        set(output "${output}/${file_name}")
    endif()

    message(STATUS "Generating ${output}")
    configure_file(${input} ${output} ${ARGN})
endfunction()

function(nx_configure_directory input output)
    file(GLOB_RECURSE files RELATIVE ${input} ${input}/*)
    foreach(file ${files})
        set(src_template_path ${input}/${file})
        if(NOT IS_DIRECTORY ${src_template_path})
            nx_configure_file(${input}/${file} ${output}/${file})
        endif()
    endforeach()
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
                    file(COPY "${entry}" DESTINATION "${CMAKE_RUNTIME_OUTPUT_DIRECTORY_${CONFIG}}")
                else()
                    file(COPY "${entry}" DESTINATION "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
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
                        file(COPY "${entry}"
                            DESTINATION "${CMAKE_LIBRARY_OUTPUT_DIRECTORY_${CONFIG}}")
                    else()
                        file(COPY "${entry}" DESTINATION "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}")
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
    file(GLOB contents ${CMAKE_CURRENT_LIST_DIR}/*)
    file(COPY ${contents} DESTINATION .
        PATTERN ".rdpack" EXCLUDE
        PATTERN "*.cmake" EXCLUDE
    )
endfunction()

# TODO Extend nx_copy function
function(nx_copy_bin_resources input output)
  file(GLOB files RELATIVE ${input} ${input}/*)
  foreach(file ${files})
    message("copying ${file}")
    nx_copy("${input}/${file}" DESTINATION "${output}")
  endforeach(file)
endfunction()
