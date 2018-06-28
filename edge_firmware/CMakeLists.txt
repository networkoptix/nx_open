set(depends
    mediaserver
    server_plugins
)

set(tar_file ${distribution_output_dir}/${artifact.name.server}.tar.gz)
set(zip_file ${distribution_output_dir}/${artifact.name.server_update}.zip)
set(installer_output_list ${tar_file} ${zip_file})

set(conf_file maven/filter-resources/build_distribution.conf)
nx_configure_file(
    ${conf_file}
    ${CMAKE_CURRENT_BINARY_DIR}
    @ONLY)
list(APPEND depends ${conf_file})

if(targetDevice STREQUAL "edge1")
    set(box_dir "isd")
else()
    set(box_dir "rpi")
endif()

nx_configure_directory(
    ${CMAKE_CURRENT_SOURCE_DIR}/${box_dir}/maven/filter-resources
    ${CMAKE_CURRENT_BINARY_DIR}
    @ONLY
    OUTPUT_FILES_VARIABLE filtered1)

nx_configure_directory(
    ${CMAKE_CURRENT_SOURCE_DIR}/${box_dir}/maven/${targetDevice}
    ${CMAKE_CURRENT_BINARY_DIR}
    @ONLY
    OUTPUT_FILES_VARIABLE filtered2)

nx_create_qt_conf(${CMAKE_CURRENT_BINARY_DIR}/qt.conf)

set(script ${CMAKE_CURRENT_SOURCE_DIR}/build_distribution.sh)

add_custom_command(
    OUTPUT ${installer_output_list}
    COMMAND ${script} --no-client
    DEPENDS ${script} ${depends} ${filtered1} ${filtered2}
    COMMENT "Creating ARM installer archives"
)

add_custom_target(arm-distribution ALL
    DEPENDS ${installer_output_list}
)
