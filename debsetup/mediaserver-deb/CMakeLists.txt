set(depends mediaserver)

set(zip_file ${distribution_output_dir}/${artifact.name.server_update}.zip)
set(deb_file ${distribution_output_dir}/${artifact.name.server}.deb)
set(installer_output_list ${zip_file} ${deb_file})

# Variables which may depend on current project and are used by build-dist.sh.
set(project.build.directory ${CMAKE_CURRENT_BINARY_DIR})
set(build.configuration "") #< Used by build-dist.sh as suffix to lib and bin dirs.
set(basedir ${CMAKE_CURRENT_SOURCE_DIR}) # For ${basedir}/../scripts

# Variables used in filtered files.
set(arch.depends ", syslinux")

nx_configure_directory(
    ${CMAKE_CURRENT_SOURCE_DIR}/maven/filter-resources
    ${CMAKE_CURRENT_BINARY_DIR}
    @ONLY
    OUTPUT_FILES_VARIABLE filtered)

set(script ${CMAKE_CURRENT_BINARY_DIR}/build-dist.sh)

add_custom_command(
    OUTPUT ${installer_output_list}
    COMMAND ${script}
    DEPENDS ${script} ${depends} ${filtered}
    COMMENT "Creating mediaserver .deb package"
)

add_custom_target(mediaserver-deb ALL
    DEPENDS ${installer_output_list}
)
