set(depends mediaserver)

# Variables used by build-dist.sh.
set(common_depends
    "libstdc++6 (>= 4.1.1)"
    "zlib1g (>= 1:1.1.4)"
    "debconf (>= 0.5) | debconf-2.0"
    "python"
    "upstart-job"
    "psmisc"
    "upstart (>= 0.6.5-8)"
    "libsm6"
    "libice6"
    "libaudio2"
    "libogg0"
    "libxfixes3"
    "libxrender1"
    "libfontconfig1"
    "libgl1-mesa-glx"
    "cifs-utils"
)
string(REPLACE ";" ", " common.depends "${common_depends}") #< Make a comma-separated list.
set(arch.depends ", syslinux")

# Variables which may depend on current project and are used by build-dist.sh.
set(project.build.directory ${CMAKE_CURRENT_BINARY_DIR})
set(basedir ${CMAKE_CURRENT_SOURCE_DIR}) # For ${basedir}/../scripts

nx_configure_directory(
    ${CMAKE_CURRENT_SOURCE_DIR}/maven/filter-resources
    ${CMAKE_CURRENT_BINARY_DIR}
    @ONLY
    OUTPUT_FILES_VARIABLE filtered)

set(zip_file ${distribution_output_dir}/${artifact.name.server_update}.zip)
set(deb_file ${distribution_output_dir}/${artifact.name.server}.deb)
set(installer_output_list ${zip_file} ${deb_file})

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
