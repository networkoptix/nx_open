set(depends
    mediaserver
    server_plugins
    testcamera
)

set(deb_dependencies_list
    "zlib1g (>= 1:1.1.4)"
    "debconf (>= 0.5) | debconf-2.0"
    "python"
    "psmisc"
    "systemd"
    "cifs-utils"
    "net-tools"
    "libcap2-bin"
    "libldap-2.4-2"
)

if(withDesktopClient)
    # For platforms with Desktop Clients, Server uses the same Qt as Client. Because of this, on
    # such platforms Server has these "strange" dependencies:
    list(APPEND deb_dependencies_list
        "libxrender1"
        "libgl1-mesa-glx"
        "libxfixes3"
        "libfontconfig1"
        "libsm6"
        "libice6"
        "libaudio2"
        "libogg0"
    )
endif()

list(JOIN deb_dependencies_list ", " deb_dependencies)

if(withRootTool)
    list(APPEND depends root_tool)
endif()

nx_configure_directory(
    ${CMAKE_CURRENT_SOURCE_DIR}/deb_files.in
    ${CMAKE_CURRENT_BINARY_DIR}
    @ONLY
    OUTPUT_FILES_VARIABLE configured_files)

nx_generate_package_json(${CMAKE_CURRENT_BINARY_DIR}/package.json
    VARIANTS "{ \"name\": \"ubuntu\", \"minimumVersion\": \"16.04\" }"
    INSTALL_SCRIPT install.sh)
list(APPEND configured_files ${CMAKE_CURRENT_BINARY_DIR}/package.json)

nx_configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/build_distribution.conf.in
    ${CMAKE_CURRENT_BINARY_DIR}/build_distribution.conf
    @ONLY)
list(APPEND configured_files ${CMAKE_CURRENT_BINARY_DIR}/build_distribution.conf)

nx_create_qt_conf(${CMAKE_CURRENT_BINARY_DIR}/qt.conf)

set(zip_files ${distribution_output_dir}/${server_update_distribution_name}.zip)
set(deb_file ${distribution_output_dir}/${server_distribution_name}.deb)

if(targetDevice STREQUAL "linux_arm32")
    list(APPEND zip_files
        ${distribution_output_dir}/${server_update_distribution_name_rpi}.zip
        ${distribution_output_dir}/${server_update_distribution_name_bananapi}.zip
    )
endif()

set(installer_output_list ${zip_files} ${deb_file})

set(script ${CMAKE_CURRENT_SOURCE_DIR}/build_distribution.sh)
add_custom_command(
    OUTPUT ${installer_output_list}
    COMMAND /bin/bash ${script}
    DEPENDS ${script} ${depends} ${configured_files}
    COMMENT "Creating mediaserver .deb package")

add_custom_target(distribution_deb_mediaserver ALL
    DEPENDS ${installer_output_list}
)
