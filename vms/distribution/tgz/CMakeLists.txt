set(depends
    mediaserver
    server_plugins
    desktop_client
    testcamera
)

nx_configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/build_distribution.conf.in
    ${CMAKE_CURRENT_BINARY_DIR}/build_distribution.conf
    @ONLY)
set(conf_configured_file ${CMAKE_CURRENT_BINARY_DIR}/build_distribution.conf)

set(tgz_file ${distribution_output_dir}/tegra.tgz)

set(installer_output_list ${tgz_file})

nx_create_qt_conf(${CMAKE_CURRENT_BINARY_DIR}/qt.conf)

set(script ${CMAKE_CURRENT_SOURCE_DIR}/build_distribution.sh)

add_custom_command(
    OUTPUT ${installer_output_list}
    COMMAND /bin/bash ${script}
    DEPENDS ${script} ${depends} ${conf_configured_file}
    COMMENT "Creating mediaserver and desktop_client .tgz archive for Tegra"
)

add_custom_target(distribution_tgz ALL
    DEPENDS ${installer_output_list}
)
