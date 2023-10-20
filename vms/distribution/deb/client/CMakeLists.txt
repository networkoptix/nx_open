## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

if(NOT openSourceBuild)
    include(openssl_signing)
endif()

set(depends
    desktop_client
    applauncher
)

# Files copied to .deb by build_distribution.sh.

file(WRITE "${CMAKE_BINARY_DIR}/bin/${launcher.version.file}" "${releaseVersion.full}")
nx_store_known_file(${CMAKE_BINARY_DIR}/bin/${launcher.version.file})

nx_copy(${vms_distribution_common_dir}/desktop_client/backgrounds
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

nx_load_deb_dependencies_from_file(${CMAKE_CURRENT_SOURCE_DIR}/deb_dependencies.yaml)

nx_configure_directory(
    ${CMAKE_CURRENT_SOURCE_DIR}/deb_files.in
    ${CMAKE_CURRENT_BINARY_DIR}
    @ONLY
    OUTPUT_FILES_VARIABLE configured_files)

nx_generate_package_json(${CMAKE_CURRENT_BINARY_DIR}/package.json
    COMPONENT client
    VARIANTS "{ \"name\": \"ubuntu\", \"minimumVersion\": \"18.04\" }")
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
nx_add_targets_to_strengthened(${installer_output_list})

if(NOT openSourceBuild)
    nx_sign_update_package(distribution_deb_client ${zip_file})
endif()
