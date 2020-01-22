set(depends
    desktop_client
    applauncher
)

# Files copied to .deb by build_distribution.sh.

file(WRITE "${CMAKE_BINARY_DIR}/bin/${launcher.version.file}" "${releaseVersion.full}")
nx_store_known_file(${CMAKE_BINARY_DIR}/bin/${launcher.version.file})

nx_copy(${CMAKE_SOURCE_DIR}/build_environment/maven/bin-resources/backgrounds
    DESTINATION ${CMAKE_BINARY_DIR})

set(plugins
    imageformats
    mediaservice
    audio
    xcbglintegrations
    platforminputcontexts
    platforms
)
foreach(plugin ${plugins})
    nx_copy(${QT_DIR}/plugins/${plugin} DESTINATION ${CMAKE_BINARY_DIR}/bin)
endforeach()

nx_copy(${QT_DIR}/qml DESTINATION ${CMAKE_BINARY_DIR}/bin)

nx_create_qt_conf(${CMAKE_CURRENT_BINARY_DIR}/qt.conf)

# Variables used for configuring files.
set(common_depends
    "zlib1g (>= 1:1.1.4)"
    "debconf (>= 0.5) | debconf-2.0"
    "libpulse-mainloop-glib0"
    "libpulse0"
    "libxi6"
    "libxrender1"
    "libxcomposite1"
    "libxfixes3"
    "libxt6"
    "libfontconfig1"
    "libfreetype6 (>= 2.6.0)"
    "libsm6"
    "libice6"
    "libaudio2"
    "libgl1-mesa-glx | libgl1"
    "libegl1-mesa | libegl1-x11"
)
string(REPLACE ";" ", " common.depends "${common_depends}") #< Make a comma-separated list.

nx_configure_directory(
    ${CMAKE_CURRENT_SOURCE_DIR}/deb_files.in
    ${CMAKE_CURRENT_BINARY_DIR}
    @ONLY
    OUTPUT_FILES_VARIABLE configured_files)

nx_generate_package_json(${CMAKE_CURRENT_BINARY_DIR}/package.json
    COMPONENT client
    VARIANTS "{ \"name\": \"ubuntu\", \"minimumVersion\": \"16.04\" }")
list(APPEND configured_files ${CMAKE_CURRENT_BINARY_DIR}/package.json)

nx_configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/build_distribution.conf.in
    ${CMAKE_CURRENT_BINARY_DIR}/build_distribution.conf
    @ONLY)
list(APPEND configured_files ${CMAKE_CURRENT_BINARY_DIR}/build_distribution.conf)

set(zip_file ${distribution_output_dir}/${client_update_distribution_name}.zip)
set(deb_file ${distribution_output_dir}/${client_distribution_name}.deb)
set(installer_output_list ${zip_file} ${deb_file})

set(script ${CMAKE_CURRENT_SOURCE_DIR}/build_distribution.sh)
add_custom_command(
    OUTPUT ${installer_output_list}
    COMMAND /bin/bash ${script}
    DEPENDS ${script} ${depends} ${configured_files}
    COMMENT "Creating desktop client .deb package"
)

add_custom_target(distribution_deb_client ALL
    DEPENDS ${installer_output_list}
)
