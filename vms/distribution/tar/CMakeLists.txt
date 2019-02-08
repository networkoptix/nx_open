set(depends
    mediaserver
    server_plugins
)

set(tar_file ${distribution_output_dir}/${server_distribution_name}.tar.gz)
set(zip_file ${distribution_output_dir}/${server_update_distribution_name}.zip)
set(installer_output_list ${tar_file} ${zip_file})

nx_configure_directory(
    ${CMAKE_CURRENT_SOURCE_DIR}/common.in
    ${CMAKE_CURRENT_BINARY_DIR}
    @ONLY
    OUTPUT_FILES_VARIABLE common_configured_files)

nx_configure_directory(
    ${CMAKE_CURRENT_SOURCE_DIR}/${targetDevice}.in
    ${CMAKE_CURRENT_BINARY_DIR}
    @ONLY
    OUTPUT_FILES_VARIABLE device_configured_files)

nx_configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/build_distribution.conf.in
    ${CMAKE_CURRENT_BINARY_DIR}/build_distribution.conf
    @ONLY)
set(conf_configured_file ${CMAKE_CURRENT_BINARY_DIR}/build_distribution.conf)

nx_create_qt_conf(${CMAKE_CURRENT_BINARY_DIR}/qt.conf)

set(script ${CMAKE_CURRENT_SOURCE_DIR}/build_distribution.sh)

add_custom_command(
    OUTPUT ${installer_output_list}
    COMMAND /bin/bash ${script} --no-client
    DEPENDS ${script} ${depends}
        ${common_configured_files} ${device_configured_files} ${conf_configured_file}
    COMMENT "Creating distribution archives"
)

add_custom_target(distribution_tar ALL
    DEPENDS ${installer_output_list}
)
