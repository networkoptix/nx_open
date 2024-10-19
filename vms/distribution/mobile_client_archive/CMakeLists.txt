## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

include(${open_source_root}/cmake/distribution/common.cmake)

message(STATUS "Generating files for Mobile Client Archive...")

nx_get_component_translation_list(mobile_client_translation_files "mobile")

set(mobile_client_archive_file
    "${distribution_output_dir}/${mobile_client_archive_distribution_name}.zip")

set(mobile_client_qml_root
    "${CMAKE_SOURCE_DIR}/open/vms/client/mobile_client/static-resources/qml/")

set(compiled_qml_directory "${CMAKE_CURRENT_BINARY_DIR}/qml")

execute_process(
    WORKING_DIRECTORY ${open_source_root}/build_utils
    COMMAND ${PYTHON_EXECUTABLE} qmldeploy.py
        "--qml-root" ${mobile_client_qml_root}
        "--qt-root" ${QT_DIR}
        "--qt-host-root" ${QT_HOST_PATH}
        "--output" ${compiled_qml_directory}
        "--prefer-compiled"
        "--skip-styles"
        "--clean"
)

file(GLOB_RECURSE mobile_client_qml_files CONFIGURE_DEPENDS "${compiled_qml_directory}/*")
nx_store_known_files(${mobile_client_qml_files})

nx_create_qt_conf(${CMAKE_CURRENT_BINARY_DIR}/qt.conf QT_PREFIX .)

set(intermediate_config_file "${CMAKE_CURRENT_BINARY_DIR}/intermediate_config.yaml")
nx_configure_file("${CMAKE_CURRENT_SOURCE_DIR}/config.yaml" "${intermediate_config_file}")

# Evaluate generator expressions in the config file.
set(config_file "${CMAKE_CURRENT_BINARY_DIR}/config.yaml")
file(GENERATE OUTPUT "${config_file}" INPUT "${intermediate_config_file}")
nx_store_known_file(${config_file})

nx_create_distribution_package(
    PACKAGE_NAME "Mobile Client Archive"
    PACKAGE_TARGET mobile_client_archive
    PACKAGE_GENERATION_SCRIPT_NAME generate_mobile_client_archive.py
    OUTPUT_FILE ${mobile_client_archive_file}
    CONFIG_FILES ${config_file}
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    DEPENDENCIES mobile_client ${config_file})
