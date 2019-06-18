if(NOT build_utils_dir)
    set(build_utils_dir "${CMAKE_SOURCE_DIR}/build_utils")
endif()

function(nx_unpack_customization_package source_directory target_directory)
    message(STATUS "Unpacking customization package from ${source_directory} to ${target_directory}")

    execute_process(
        COMMAND ${PYTHON_EXECUTABLE}
            ${build_utils_dir}/customization/pack2.py unpack
            ${source_directory}
            ${target_directory}
    )
endfunction()

nx_rdep_add_package(any/customization_pack-${customization}
    PATH_VARIABLE customization_package_directory)

set(customization_dir "${CMAKE_BINARY_DIR}/customization/${customization}")
nx_unpack_customization_package(
    ${customization_package_directory}
    ${customization_dir})
