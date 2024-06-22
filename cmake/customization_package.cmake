## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

include_guard(GLOBAL)

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
    message(STATUS "Got customization id = \"${customization_id}\"")
endfunction()

macro(nx_load_customization_package)
    if(customizationPackageFile)
        set(customization_package_file ${customizationPackageFile})
        get_filename_component(customization_name "${customization_package_file}" NAME_WE)
    elseif(CONAN_CUSTOMIZATION_ROOT)
        set(customization_package_file "${CONAN_CUSTOMIZATION_ROOT}/package.zip")
        set(customization_name ${customization})
    else()
        message(FATAL_ERROR "Customization Package not found in Conan.")
    endif()

    if(NOT EXISTS "${customization_package_file}")
        message(FATAL_ERROR
            "The Customization Package file (${customization_package_file}) was not found.")
    endif()

    set(customization_dir "${CMAKE_BINARY_DIR}/customization/${customization_name}")
    set(customization_unpack_log_file
        "${CMAKE_BINARY_DIR}/build_logs/unpack-${customization_name}.log")
    _unpack_customization_package(
        ${customization_package_file}
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

_set_customization_from_file()
