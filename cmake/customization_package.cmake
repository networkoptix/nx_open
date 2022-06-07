## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

include_guard(GLOBAL)

set(customizationPackageFile "" CACHE STRING "Customization package archive")

function(nx_unpack_customization_package source_file target_directory log_file)
    message(STATUS "Unpacking customization package from ${source_file} to ${target_directory}")
    execute_process(
        COMMAND ${PYTHON_EXECUTABLE}
            ${open_build_utils_dir}/customization/pack2.py unpack
            ${source_file}
            ${target_directory}
            -l ${log_file}
            --clean
        RESULT_VARIABLE unpack_result
    )

    if(NOT unpack_result STREQUAL "0")
        message(FATAL_ERROR "error: Customization unpacking failed. See ${log_file} for details.")
    endif()
endfunction()

function(nx_store_customization_package target_directory)
    message(STATUS "Listing customization package contents in the ${target_directory}")

    set(listed_files)
    execute_process(
        COMMAND ${PYTHON_EXECUTABLE}
            ${open_build_utils_dir}/customization/pack2.py list
            ${target_directory}
        OUTPUT_VARIABLE listed_files
        RESULT_VARIABLE list_result
    )

    if(NOT list_result STREQUAL "0")
        message(FATAL_ERROR "error: Customization listing failed. See ${log_file} for details.")
    endif()

    string(REPLACE "\n" ";" listed_files ${listed_files})
    nx_store_known_files(${listed_files})
endfunction()

if(customizationPackageFile)
    set(customization_package_file ${customizationPackageFile})
    get_filename_component(customization_name "${customization_package_file}" NAME_WE)
elseif(CONAN_CUSTOMIZATION_ROOT)
    set(customization_package_file "${CONAN_CUSTOMIZATION_ROOT}/package.zip")
    set(customization_name ${customization})
else()
    if(NOT openSourceBuild)
        set(error_message "No customization package is found. Check if it is downloaded by Rdep.")
    else()
        set(error_message
            "Customization package file not specified. Download the Nx Meta customization package "
            "from https://meta.nxvms.com, or download a different Powered-by-Nx customization package "
            "using the corresponding Developer account, and specify the path to it with "
            "-DcustomizationPackageFile=<filename>"
        )
    endif()

    message(FATAL_ERROR "\n" ${error_message} "\n")
endif()

if(NOT EXISTS "${customization_package_file}")
    message(FATAL_ERROR
        "The customization package file (${customization_package_file}) was not found")
endif()

set(customization_dir "${CMAKE_BINARY_DIR}/customization/${customization_name}")
set(customization_unpack_log_file
    "${CMAKE_BINARY_DIR}/build_logs/unpack-${customization_name}.log")
nx_unpack_customization_package(
    ${customization_package_file}
    ${customization_dir}
    ${customization_unpack_log_file})


set(customization_cmake ${customization_dir}/generated.cmake)
nx_json_to_cmake(
    ${customization_dir}/description.json
    ${customization_cmake}
    customization)

nx_store_customization_package(${customization_dir})
nx_store_known_file(${customization_unpack_log_file})
nx_store_known_file(${customization_cmake})

include(${customization_cmake})

if(NOT customization)
    message(STATUS
        "Empty \"customization\" variable. Getting its value from from the customization package: "
        "\"${customization.id}\"")
    set(customization ${customization.id})
elseif(NOT "${customization}" STREQUAL "${customization.id}")
    message(FATAL_ERROR "Customization package integrity check failed.\
    Expected value: \"${customization}\". Actual value: \"${customization.id}\"")
endif()
