## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

include_guard(GLOBAL)

include(utils)
include(${PROJECT_SOURCE_DIR}/cmake/windows_signing.cmake)

# See nx_wix_light(). CMake ignores CMAKE_JOB_POOLS once the property is set, so it is kept here.
get_property(job_pools_set GLOBAL PROPERTY JOB_POOLS SET)
if(NOT job_pools_set)
    set_property(GLOBAL PROPERTY JOB_POOLS ${CMAKE_JOB_POOLS})
endif()
set_property(GLOBAL APPEND PROPERTY JOB_POOLS nx_wix_payload_signing=1)

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


# Call Wix linker (light) for the source files. With STRENGTHEN, light and the payload signing
# rerun on a change of any transitive dependency (see nx_add_targets_to_strengthened()); the
# payload signing must rerun whenever light does, or light packs a relinked unsigned file.
function(nx_wix_light target_file)
    set(options
        STRENGTHEN)
    set(oneValueArgs
        LOCALIZATION_FILE)
    set(multiValueArgs
        SOURCE_FILES
        EXTENSIONS
        DEPENDS)
    cmake_parse_arguments(WXS "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

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

    set(light_depends
        ${WXS_SOURCE_FILES}
        ${WXS_LOCALIZATION_FILE}
        ${WXS_DEPENDS})

    # Sign the payload right before light packs it, in a step of its own: the payloads of the
    # installers overlap, so the signing steps run one at a time, while the lights run in
    # parallel. wix_signing_package_type is set by the distribution directories for the signing
    # statistics.
    if(codeSigning)
        if(NOT CMAKE_GENERATOR MATCHES "Ninja")
            message(FATAL_ERROR
                "Signing the WiX payload requires a Ninja generator, which supports job pools.")
        endif()

        set(sign_payload_commands "")
        nx_get_wix_payload_sign_commands(sign_payload_commands
            PACKAGE_TYPE ${wix_signing_package_type}
            WIXOBJ_FILES ${WXS_SOURCE_FILES})

        get_filename_component(target_name ${target_file} NAME)
        set(stamp_file ${CMAKE_CURRENT_BINARY_DIR}/${target_name}.payload_signed)
        add_custom_command(
            COMMENT "[Wix] Sign the payload of ${target_file}"
            DEPENDS ${light_depends}
            ${sign_payload_commands}
            COMMAND ${CMAKE_COMMAND} -E touch ${stamp_file}
            OUTPUT ${stamp_file}
            JOB_POOL nx_wix_payload_signing
            VERBATIM
        )
        list(APPEND light_depends ${stamp_file})
        if(WXS_STRENGTHEN)
            nx_add_targets_to_strengthened(${stamp_file})
        endif()
    endif()

    add_custom_command(
        COMMENT "[Wix] Light ${target_file}"
        DEPENDS ${light_depends}
        COMMAND ${light_command}
        OUTPUT ${target_file}
        VERBATIM
    )
    if(WXS_STRENGTHEN)
        nx_add_targets_to_strengthened(${target_file})
    endif()
endfunction()


function(nx_wix_add_light_target target target_file)
    nx_wix_light(${target_file} ${ARGN})
    add_custom_target(${target} DEPENDS ${target_file})
    set_target_properties(${target} PROPERTIES FOLDER distribution)
endfunction()


function(nx_wix_sign_msi target_file)
    set(oneValueArgs
        SOURCE_FILE
        PACKAGE_TYPE)
    cmake_parse_arguments(WXS "" "${oneValueArgs}" "" ${ARGN})

    set(signCommand "")
    nx_get_windows_sign_command(signCommand PACKAGE_TYPE ${WXS_PACKAGE_TYPE})

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
        TARGET_FILE
        PACKAGE_TYPE)
    cmake_parse_arguments(WXS "" "${oneValueArgs}" "" ${ARGN})

    get_filename_component(filename ${WXS_SOURCE_FILE} NAME)

    set(engineExeFile "${CMAKE_CURRENT_BINARY_DIR}/${filename}-engine.exe")
    set(signCommand "")
    nx_get_windows_sign_command(signCommand PACKAGE_TYPE ${WXS_PACKAGE_TYPE})

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


function(nx_wix_generate_wxs_file target_file)
    set(oneValueArgs
        ID
        SOURCE
        DIRECTORY)
    set(multiValueArgs
        FILENAMES)
    cmake_parse_arguments(WXS "" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    set(WXS_COMPONENTS)
    foreach(filename ${WXS_FILENAMES})
        string(APPEND WXS_COMPONENTS
            "<Component><File Name=\"${filename}\" KeyPath=\"yes\"/></Component>\n")
    endforeach()
    nx_configure_file(${CMAKE_CURRENT_FUNCTION_LIST_DIR}/wix_utils/wxs_component_group.wxs.in
        ${target_file})

endfunction()
