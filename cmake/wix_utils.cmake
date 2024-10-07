## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

include_guard(GLOBAL)

include(utils)
include(${PROJECT_SOURCE_DIR}/cmake/windows_signing.cmake)

find_program(heat_executable heat HINTS ${CONAN_WIX_ROOT}/bin NO_DEFAULT_PATH)
if(NOT heat_executable)
    message(FATAL_ERROR "Cannot find heat.")
endif()

find_program(candle_executable candle HINTS ${CONAN_WIX_ROOT}/bin NO_DEFAULT_PATH)
if(NOT candle_executable)
    message(FATAL_ERROR "Cannot find candle.")
endif()

find_program(light_executable light HINTS ${CONAN_WIX_ROOT}/bin NO_DEFAULT_PATH)
if(NOT light_executable)
    message(FATAL_ERROR "Cannot find light.")
endif()

find_program(insignia_executable insignia HINTS ${CONAN_WIX_ROOT}/bin NO_DEFAULT_PATH)
if(NOT insignia_executable)
    message(FATAL_ERROR "Cannot find insignia.")
endif()


# Call Wix compiler (candle) for the source file. If file is not STATIC, it will be configured.
# Variables should be passed in format 'SourceDir=<actual_dir_path>'.
function(nx_wix_candle_ext target_file)
    set(options
        STATIC)
    set(oneValueArgs
        SOURCE_FILE)
    set(multiValueArgs
        VARIABLES
        EXTENSIONS
        DEPENDS)
    cmake_parse_arguments(WXS "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    set(source_file ${WXS_SOURCE_FILE})
    if(NOT WXS_STATIC)
        get_filename_component(filename ${WXS_SOURCE_FILE} NAME)
        set(source_file ${CMAKE_CURRENT_BINARY_DIR}/${filename})
        nx_configure_file(${WXS_SOURCE_FILE} ${source_file})
    endif()

    set(candle_command ${candle_executable}
        -arch x64
        -nologo
        -out ${target_file}
        ${source_file})

    foreach(variable ${WXS_VARIABLES})
        list(APPEND candle_command -d${variable})
    endforeach()

    foreach(extension ${WXS_EXTENSIONS})
        list(APPEND candle_command -ext ${extension}.dll)
    endforeach()

    add_custom_command(
        COMMENT "[Wix] Candle ${target_file}"
        MAIN_DEPENDENCY ${source_file}
        COMMAND ${candle_command}
        OUTPUT ${target_file}
        DEPENDS
            ${WXS_DEPENDS}
        VERBATIM
    )
endfunction()


# Simple alias for the nx_wix_candle_ext, supposing source and target files are named the same way
# and located in the default directories
function(nx_wix_candle filename)
    nx_wix_candle_ext(${CMAKE_CURRENT_BINARY_DIR}/${filename}.wixobj
        SOURCE_FILE ${CMAKE_CURRENT_SOURCE_DIR}/${filename}.wxs
        ${ARGN})
endfunction()


# Call Wix generator (heat) for the source directory.
function(nx_wix_heat target_file)
    set(oneValueArgs
        SOURCE_DIR
        TARGET_DIR_ALIAS
        COMPONENT_GROUP)
    cmake_parse_arguments(WXS "" "${oneValueArgs}" "" ${ARGN})

    set(wxs_file ${CMAKE_CURRENT_BINARY_DIR}/${WXS_COMPONENT_GROUP}.wxs)
    set(source_dir_var_name "${WXS_COMPONENT_GROUP}_dir")
    set(source_dir_alias "var.${source_dir_var_name}")

    execute_process(
        COMMAND ${heat_executable}
            dir ${WXS_SOURCE_DIR}
            -out ${wxs_file}.copy
            -var ${source_dir_alias}
            -dr ${WXS_TARGET_DIR_ALIAS}
            -cg ${WXS_COMPONENT_GROUP}
            -ag
            -srd
            -sfrag
            -sreg
            -suid
            -nologo
        RESULT_VARIABLE heat_result)
    if(NOT heat_result EQUAL 0)
        message(FATAL_ERROR "[Wix] Cannot heat directory ${WXS_SOURCE_DIR}")
    endif()

    file(TIMESTAMP "${wxs_file}" orig_ts)
    execute_process(
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${wxs_file}.copy ${wxs_file}
        RESULT_VARIABLE result)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "[Wix] Cannot create ${wxs_file}")
    endif()
    file(REMOVE ${wxs_file}.copy)
    file(TIMESTAMP "${wxs_file}" new_ts)

    if(NOT ${orig_ts} STREQUAL ${new_ts})
        message(STATUS "[Wix] Generated ${wxs_file}")
    endif()

    file(GLOB_RECURSE source_files ${WXS_SOURCE_DIR}/*)

    nx_wix_candle_ext(${target_file}
        STATIC
        SOURCE_FILE ${wxs_file}
        DEPENDS ${source_files}
        VARIABLES
            ${source_dir_var_name}=${WXS_SOURCE_DIR}
    )
endfunction()


# Call Wix linker (light) for the source files.
function(nx_wix_light target_file)
    set(oneValueArgs
        LOCALIZATION_FILE)
    set(multiValueArgs
        SOURCE_FILES
        EXTENSIONS
        DEPENDS)
    cmake_parse_arguments(WXS "" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    set(light_command ${light_executable}
        -nologo
        -cc ${CMAKE_CURRENT_BINARY_DIR}/cab_cache
        -reusecab
        -sval
        -spdb
        -out ${target_file}
        ${WXS_SOURCE_FILES})

    if(WXS_LOCALIZATION_FILE)
        list(APPEND light_command -loc ${WXS_LOCALIZATION_FILE})
    endif()

    foreach(extension ${WXS_EXTENSIONS})
        list(APPEND light_command -ext ${extension})
    endforeach()

    add_custom_command(
        COMMENT "[Wix] Light ${target_file}"
        DEPENDS
            ${WXS_SOURCE_FILES}
            ${WXS_LOCALIZATION_FILE}
            ${WXS_DEPENDS}
        COMMAND ${light_command}
        OUTPUT ${target_file}
        VERBATIM
    )
endfunction()


function(nx_wix_add_light_target target target_file)
    nx_wix_light(${target_file} ${ARGN})
    add_custom_target(${target} DEPENDS ${target_file})
    set_target_properties(${target} PROPERTIES FOLDER distribution)
endfunction()


function(nx_wix_sign_msi target_file)
    set(oneValueArgs
        SOURCE_FILE)
    cmake_parse_arguments(WXS "" "${oneValueArgs}" "" ${ARGN})

    set(signCommand "")
    nx_get_windows_sign_command(signCommand)

    add_custom_command(
        COMMENT "[Wix] Signing ${target_file}"
        COMMAND ${signCommand}
            ${WXS_SOURCE_FILE}
            --output ${target_file}
        OUTPUT ${target_file}
        MAIN_DEPENDENCY
            ${WXS_SOURCE_FILE}
    )
endfunction()


function(nx_wix_add_signed_exe_target target)
    set(oneValueArgs
        SOURCE_FILE
        TARGET_FILE)
    cmake_parse_arguments(WXS "" "${oneValueArgs}" "" ${ARGN})

    get_filename_component(filename ${WXS_SOURCE_FILE} NAME)

    set(engineExeFile "${CMAKE_CURRENT_BINARY_DIR}/${filename}-engine.exe")
    set(signCommand "")
    nx_get_windows_sign_command(signCommand)

    add_custom_command(
        COMMAND ${insignia_executable}
            -ib ${WXS_SOURCE_FILE}
            -o ${engineExeFile}

        COMMAND ${signCommand} ${engineExeFile}
            --output ${engineExeFile}

        COMMAND ${insignia_executable}
            -ab ${engineExeFile} ${WXS_SOURCE_FILE}
            -o ${WXS_TARGET_FILE}

        COMMAND ${signCommand} ${WXS_TARGET_FILE}
            --output ${WXS_TARGET_FILE}
        OUTPUT ${WXS_TARGET_FILE}
        MAIN_DEPENDENCY
            ${WXS_SOURCE_FILE}
    )

    add_custom_target(${target} ALL
        DEPENDS
            ${WXS_TARGET_FILE}
        COMMENT "[Wix] Signing ${WXS_TARGET_FILE}"
    )
    set_target_properties(${target} PROPERTIES FOLDER distribution)

endfunction()
