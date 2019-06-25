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
        RESULT_VARIABLE unpack_result
    )

    if(NOT unpack_result STREQUAL "0")
        message(FATAL_ERROR "error: Customization unpacking failed. See ${log_file} for details.")
    endif()
endfunction()

nx_rdep_add_package(any/customization_pack-${customization}
    PATH_VARIABLE customization_package_directory)

set(customization_dir "${CMAKE_BINARY_DIR}/customization/${customization}")
set(customization_unpack_log_file "${CMAKE_BINARY_DIR}/build_logs/unpack-${customization}.log")
nx_unpack_customization_package(
    ${customization_package_directory}
    ${customization_dir}
    ${customization_unpack_log_file})

nx_store_known_file(${customization_unpack_log_file})
