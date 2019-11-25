if(NOT build_utils_dir)
    set(build_utils_dir "${CMAKE_SOURCE_DIR}/build_utils")
endif()

function(nx_unpack_customization_package source_directory target_directory log_file)
    message(STATUS "Unpacking customization package from ${source_directory} to ${target_directory}")
    execute_process(
        COMMAND ${PYTHON_EXECUTABLE}
            ${build_utils_dir}/customization/pack2.py unpack
            ${source_directory}/package.zip
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
            ${build_utils_dir}/customization/pack2.py list
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

if(NOT customization_package_directory)
    message(FATAL_ERROR "Customization package was not loaded")
endif()

set(customization_dir "${CMAKE_BINARY_DIR}/customization/${customization}")
set(customization_unpack_log_file "${CMAKE_BINARY_DIR}/build_logs/unpack-${customization}.log")
nx_unpack_customization_package(
    ${customization_package_directory}
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

if(NOT ${customization} STREQUAL ${customization.id})
    message(FATAL_ERROR "Customization package integrity check failed.\
    Expected value: \"${customization}\". Actual value: \"${customization.id}\"")
endif()
