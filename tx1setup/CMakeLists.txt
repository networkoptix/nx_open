set(depends
    mediaserver
    server_plugins
    desktop_client
    testcamera
)

nx_configure_directory(
    ${CMAKE_CURRENT_SOURCE_DIR}/maven/filter-resources
    ${CMAKE_CURRENT_BINARY_DIR}
    @ONLY
    OUTPUT_FILES_VARIABLE filtered)

set(tgz_file ${distribution_output_dir}/tegra.tgz)
set(installer_output_list ${tgz_file} ${deb_file})

set(script ${CMAKE_CURRENT_BINARY_DIR}/build-dist.sh)
add_custom_command(
    OUTPUT ${installer_output_list}
    COMMAND ${script}
    DEPENDS ${script} ${depends} ${filtered}
    COMMENT "Creating mediaserver and desktop_client .tgz archive for Tegra"
)

add_custom_target(tegra-tgz ALL
    DEPENDS ${installer_output_list}
)
