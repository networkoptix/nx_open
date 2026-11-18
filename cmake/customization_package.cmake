## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

include_guard(GLOBAL)

include(${open_source_root}/cmake/find_python.cmake)
include(${open_source_root}/cmake/utils.cmake)

set(customizationPackageFile "" CACHE STRING "Customization package archive")

function(_unpack_customization_package source_file target_directory log_file)
    message(STATUS "Unpacking customization package from ${source_file} to ${target_directory}")
    execute_process(
        COMMAND ${PYTHON_EXECUTABLE}
            ${open_build_utils_dir}/customization/pack.py unpack
            ${source_file}
            ${target_directory}
            -l ${log_file}
            --clean
        RESULT_VARIABLE unpack_result
    )

    if(NOT unpack_result STREQUAL "0")
        message(FATAL_ERROR "Customization unpacking failed. See ${log_file} for details.")
    endif()
endfunction()

function(_store_customization_package target_directory)
    message(STATUS "Listing customization package contents in the ${target_directory}")

    set(listed_files)
    execute_process(
        COMMAND ${PYTHON_EXECUTABLE}
            ${open_build_utils_dir}/customization/pack.py list
            ${target_directory}
        OUTPUT_VARIABLE listed_files
        RESULT_VARIABLE list_result
    )

    if(NOT list_result STREQUAL "0")
        message(FATAL_ERROR "Customization listing failed.")
    endif()

    string(REPLACE "\n" ";" listed_files ${listed_files})
    nx_store_known_files(${listed_files})
endfunction()

function(_set_customization_from_file)
    if(NOT customizationPackageFile)
        return()
    endif()

    message(WARNING
        "-Dcustomization=... parameter is ignored: -DcustomizationPackageFile=... is specified")

    message(STATUS "Getting customization id from ${customizationPackageFile}")
    execute_process(
        COMMAND ${PYTHON_EXECUTABLE}
            ${open_build_utils_dir}/customization/pack.py get_value
            ${customizationPackageFile}
            id
        OUTPUT_VARIABLE customization_id
        ERROR_VARIABLE get_value_error
        RESULT_VARIABLE get_value_resilt
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    if(NOT get_value_resilt STREQUAL "0")
        message(FATAL_ERROR "Customization listing failed: ${get_value_error}.")
    endif()

    set(customization ${customization_id} PARENT_SCOPE)
endfunction()

macro(nx_load_customization_package)
    if(NOT EXISTS "${customizationPackageFile}")
        message(FATAL_ERROR
            "The customization package file does not exist: ${customizationPackageFile}")
    endif()

    set(customization_dir "${CMAKE_BINARY_DIR}/customization/${customization}")
    set(customization_unpack_log_file
        "${CMAKE_BINARY_DIR}/build_logs/unpack-${customization}.log")
    _unpack_customization_package(
        ${customizationPackageFile}
        ${customization_dir}
        ${customization_unpack_log_file}
    )

    set(customization_cmake ${customization_dir}/generated.cmake)
    nx_json_to_cmake(
        ${customization_dir}/description.json
        ${customization_cmake}
        customization
    )

    _store_customization_package(${customization_dir})
    nx_store_known_file(${customization_unpack_log_file})
    nx_store_known_file(${customization_cmake})

    include(${customization_cmake})

    if(NOT customization)
        message(STATUS
            "Empty \"customization\" variable. Getting its value from the customization package: "
            "\"${customization.id}\""
        )
        set(customization ${customization.id})
    elseif(NOT "${customization}" STREQUAL "${customization.id}")
        message(FATAL_ERROR "Customization package integrity check failed.\
            Expected value: \"${customization}\". Actual value: \"${customization.id}\""
        )
    endif()
endmacro()

# Loads a customization package into <out_prefix>.* variables, leaving customization.* untouched.
# A macro so that the variables from the included generated.cmake reach the caller.
macro(nx_load_source_customization_package source_id artifactory_path out_prefix)
    set(_source_dir "${CMAKE_BINARY_DIR}/customization/${source_id}")
    set(_source_file "${_source_dir}.zip")
    set(_source_unpack_log "${CMAKE_BINARY_DIR}/build_logs/unpack-${source_id}.log")

    if(NOT EXISTS "${_source_file}")
        nx_download_from_nx_artifactory(
            "${artifactory_path}/${source_id}/package.zip"
            "${_source_file}"
            RESULT _source_download_result)
        if(NOT _source_download_result AND NOT EXISTS "${_source_file}")
            message(FATAL_ERROR "Cannot download source customization package \"${source_id}\".")
        endif()
    endif()

    _unpack_customization_package("${_source_file}" "${_source_dir}" "${_source_unpack_log}")

    set(_source_cmake "${_source_dir}/generated.cmake")
    nx_json_to_cmake("${_source_dir}/description.json" "${_source_cmake}" ${out_prefix})

    _store_customization_package("${_source_dir}")
    nx_store_known_file("${_source_file}")
    nx_store_known_file("${_source_unpack_log}")
    nx_store_known_file("${_source_cmake}")

    include("${_source_cmake}")
endmacro()

function(_download_customization_package artifactory_path customization_file)
    nx_download_from_nx_artifactory(
        "${artifactory_path}/${customization}/package.zip"
        ${customization_file}
        RESULT result)

    if(result)
        nx_store_known_file(${customization_file})
        message(STATUS "Successfully downloaded customization to ${customization_file}")
    elseif(EXISTS ${customization_file})
        message(STATUS
            "Cannot download customization. Using the existing file ${customization_file}.")
    else()
        message(FATAL_ERROR "Cannot download customization package.")
    endif()
endfunction()

function(nx_fetch_customization_package artifactory_path)
    if(customizationPackageFile)
        return()
    endif()

    set(customizationPackageFile "${CMAKE_BINARY_DIR}/customization/${customization}.zip")
    _download_customization_package(${artifactory_path} ${customizationPackageFile})
    nx_expose_to_parent_scope(customizationPackageFile)
endfunction()

_set_customization_from_file()
