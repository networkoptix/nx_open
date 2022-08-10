## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

if(NOT openSourceBuild)
    include(macosx_notarization)
    include(openssl_signing)
endif()

set(dmg_file ${distribution_output_dir}/${client_distribution_name}.dmg)
set(update_file ${distribution_output_dir}/${client_update_distribution_name}.zip)
if(arch STREQUAL x64)
    string(REPLACE macos_x64 macos compatibility_update_file ${client_update_distribution_name})
    set(compatibility_update_file ${distribution_output_dir}/${compatibility_update_file}.zip)
endif()

set(configured_files_dir ${CMAKE_CURRENT_BINARY_DIR}/configured_files_dir)

nx_configure_directory(
    ${CMAKE_CURRENT_SOURCE_DIR}/resources.in
    ${configured_files_dir}
    @ONLY
    OUTPUT_FILES_VARIABLE configured_files
)

nx_generate_package_json(${CMAKE_CURRENT_BINARY_DIR}/package.json COMPONENT client)
list(APPEND configured_files ${CMAKE_CURRENT_BINARY_DIR}/package.json)

set(app_icon ${customization_dir}/icons/macosx/logo.icns)
set(installer_icon ${customization_dir}/icons/macosx/installer_icon.icns)
if(NOT EXISTS ${installer_icon})
    set(installer_icon ${app_icon})
endif()

nx_configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/build_distribution.conf.in
    ${CMAKE_CURRENT_BINARY_DIR}/build_distribution.conf
    @ONLY)
list(APPEND configured_files ${CMAKE_CURRENT_BINARY_DIR}/build_distribution.conf)

nx_configure_directory(
    ${CMAKE_CURRENT_SOURCE_DIR}/resources
    ${configured_files_dir}
    COPYONLY
    OUTPUT_FILES_VARIABLE copied_files
)

nx_configure_directory(
    ${customization_dir}/client-dmg
    ${configured_files_dir}
    COPYONLY
    OUTPUT_FILES_VARIABLE customization_files
)

file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/${launcher.version.file} "${releaseVersion.full}")
nx_store_known_file(${CMAKE_CURRENT_BINARY_DIR}/${launcher.version.file})

# MacOS uses custom qt.conf (like in windows)
nx_copy(${CMAKE_CURRENT_SOURCE_DIR}/qt.conf DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

add_custom_command(
    DEPENDS desktop_client applauncher
        ${configured_files} ${copied_files} ${customization_files}
        ${app_icon} ${installer_icon}
    COMMENT "Creating ${dmg_file}"
    OUTPUT ${dmg_file} ${update_file} ${compatibility_update_file}

    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_CURRENT_BINARY_DIR}/dmg-folder
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${configured_files_dir} ${CMAKE_CURRENT_BINARY_DIR}

    # Copying some files and directories for maven compatibility.

    # TODO: Eliminate this copying; adjust python script accordingly.

    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/qml
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/plugins/imageformats
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/plugins/platforms
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/plugins/audio
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/plugins/mediaservice

    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${QT_DIR}/qml ${CMAKE_BINARY_DIR}/bin/qml
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${QT_DIR}/plugins/imageformats ${CMAKE_BINARY_DIR}/bin/imageformats
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${QT_DIR}/plugins/platforms ${CMAKE_BINARY_DIR}/bin/platforms
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${QT_DIR}/plugins/audio ${CMAKE_BINARY_DIR}/bin/audio
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${QT_DIR}/plugins/mediaservice ${CMAKE_BINARY_DIR}/bin/mediaservice
    COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_CURRENT_BINARY_DIR}/${launcher.version.file} ${CMAKE_BINARY_DIR}/bin

    COMMAND /bin/bash ${CMAKE_CURRENT_SOURCE_DIR}/build_distribution.sh

    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/qml
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/plugins/imageformats
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/plugins/platforms
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/plugins/audio
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/plugins/mediaservice
    COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_BINARY_DIR}/bin/${launcher.version.file}
)
add_custom_target(distribution_dmg_client ALL
    DEPENDS ${dmg_file} ${update_file} ${compatibility_update_file}
)
nx_add_targets_to_strengthened(
    ${dmg_file} ${update_file} ${compatibility_update_file}
)

if(NOT openSourceBuild)
    if(codeSigning)
        prepare_mac_keychain(distribution_dmg_client)
    endif()

    nx_create_openssl_signature(distribution_dmg_client ${update_file})
    if(compatibility_update_file)
        nx_create_openssl_signature(distribution_dmg_client ${compatibility_update_file})
    endif()
endif()
