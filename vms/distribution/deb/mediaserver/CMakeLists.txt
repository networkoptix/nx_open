set(depends
    mediaserver
    server_plugins
    testcamera
)

if(withRootTool)
    list(APPEND depends root_tool)
endif()

# Variables used for configuring files.
set(common_depends
    "zlib1g (>= 1:1.1.4)"
    "debconf (>= 0.5) | debconf-2.0"
    "python"
    "psmisc"
    "systemd | upstart (>= 0.6.5-8)"
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

if(cloudGroup STREQUAL "prod")
    set(cloud_group_suffix)
else()
    set(cloud_group_suffix "-${cloudGroup}")
endif()

set(conflicting_packages
    "${deb.customization.company.name}-mediaserver-1.2 (<< VERSION)")
string(APPEND conflicting_packages
    ",\n ${deb.customization.company.name}-entcontroller (<< VERSION)")
foreach(suffix "" "-test" "-demo" "-dev")
    if(NOT "${suffix}" STREQUAL "${cloud_group_suffix}")
        string(APPEND conflicting_packages
            ",\n ${deb.customization.company.name}-mediaserver${suffix}")
    endif()
endforeach()

nx_configure_directory(
    ${CMAKE_CURRENT_SOURCE_DIR}/deb_files.in
    ${CMAKE_CURRENT_BINARY_DIR}
    @ONLY
    OUTPUT_FILES_VARIABLE configured_files)

nx_configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/build_distribution.conf.in
    ${CMAKE_CURRENT_BINARY_DIR}/build_distribution.conf
    @ONLY)
list(APPEND configured_files ${CMAKE_CURRENT_BINARY_DIR}/build_distribution.conf)

nx_create_qt_conf(${CMAKE_CURRENT_BINARY_DIR}/qt.conf)

set(zip_file ${distribution_output_dir}/${server_update_distribution_name}.zip)
set(deb_file ${distribution_output_dir}/${server_distribution_name}.deb)
set(installer_output_list ${zip_file} ${deb_file})

set(script ${CMAKE_CURRENT_SOURCE_DIR}/build_distribution.sh)
add_custom_command(
    OUTPUT ${installer_output_list}
    COMMAND /bin/bash ${script}
    DEPENDS ${script} ${depends} ${configured_files}
    COMMENT "Creating mediaserver .deb package"
)

add_custom_target(distribution_deb_mediaserver ALL
    DEPENDS ${installer_output_list}
)
