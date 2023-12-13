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

nx_generate_package_json(${CMAKE_CURRENT_BINARY_DIR}/package.json
    COMPONENT client
    VARIANTS "{ \"minimumVersion\": \"10.16\" }")
list(APPEND configured_files ${CMAKE_CURRENT_BINARY_DIR}/package.json)

set(app_icon ${customization_dir}/icons/macosx/logo.icns)
set(installer_icon ${customization_dir}/icons/macosx/installer_icon.icns)
if(NOT EXISTS ${installer_icon})
    set(installer_icon ${app_icon})
endif()

set(distribution_config_file "${CMAKE_CURRENT_BINARY_DIR}/build_distribution.conf")
nx_configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/build_distribution.conf.in
    ${distribution_config_file}
    @ONLY)
list(APPEND configured_files ${distribution_config_file})

nx_configure_directory(
    ${CMAKE_CURRENT_SOURCE_DIR}/resources
    ${configured_files_dir}
    COPYONLY
    OUTPUT_FILES_VARIABLE copied_files
)

nx_configure_directory(
    ${customization_dir}/common-dmg
    ${configured_files_dir}
    COPYONLY
    OUTPUT_FILES_VARIABLE customization_common_files
)

nx_configure_directory(
    ${customization_dir}/client-dmg
    ${configured_files_dir}
    COPYONLY
    OUTPUT_FILES_VARIABLE customization_client_files
)

file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/${launcher.version.file} "${releaseVersion.full}")
nx_store_known_file(${CMAKE_CURRENT_BINARY_DIR}/${launcher.version.file})

# MacOS uses custom qt.conf (like in windows)
nx_copy(${CMAKE_CURRENT_SOURCE_DIR}/qt.conf DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

add_custom_command(
    DEPENDS
        desktop_client
        applauncher
        ${configured_files}
        ${copied_files}
        ${customization_common_files}
        ${customization_client_files}
        ${app_icon}
        ${installer_icon}
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
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/plugins/tls
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/plugins/multimedia

    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${QT_DIR}/qml ${CMAKE_BINARY_DIR}/bin/qml
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${QT_DIR}/plugins/imageformats ${CMAKE_BINARY_DIR}/bin/imageformats
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${QT_DIR}/plugins/platforms ${CMAKE_BINARY_DIR}/bin/platforms
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${QT_DIR}/plugins/tls ${CMAKE_BINARY_DIR}/bin/tls
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${QT_DIR}/plugins/multimedia ${CMAKE_BINARY_DIR}/bin/multimedia
    COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_CURRENT_BINARY_DIR}/${launcher.version.file} ${CMAKE_BINARY_DIR}/bin

    COMMAND /bin/bash ${CMAKE_CURRENT_SOURCE_DIR}/build_distribution.sh

    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/qml
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/plugins/imageformats
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/plugins/platforms
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/plugins/tls
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/plugins/multimedia
    COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_BINARY_DIR}/bin/${launcher.version.file}
)
add_custom_target(distribution_dmg_client ALL
    DEPENDS ${dmg_file} ${update_file} ${compatibility_update_file}
)

if(CMAKE_BUILD_TYPE MATCHES "RelWithDebInfo")
    set(client_debug_file "${distribution_output_dir}/${client_debug_distribution_name}.zip")
    nx_create_distribution_package(
        PACKAGE_NAME "client debug"
        PACKAGE_TARGET client_debug
        PACKAGE_GENERATION_SCRIPT_NAME generate_client_debug_package.py
        OUTPUT_FILE ${client_debug_file}
        CONFIG_FILES ${distribution_config_file}
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        DEPENDENCIES ${dmg_file})

    if(withDesktopClient OR withMediaServer OR withTestCamera)
        set(libs_debug_dependencies
            nx_network
            nx_pathkit
            nx_utils
            nx_fusion
            nx_reflect
            nx_zip
            nx_vms_api
            nx_vms_common
            nx_vms_rules
            nx_vms_update
            nx_vms_utils
            nx_branding
            nx_build_info
            nx_monitoring
            nx_media_core
            nx_codec
            nx_rtp
            nx_kit
            udt
        )

        if(TARGET nx_media)
            list(APPEND libs_debug_dependencies nx_media)
        endif()
        if(TARGET nx_pathkit)
            list(APPEND libs_debug_dependencies nx_pathkit)
        endif()
        if(TARGET nx_vms_json_rpc)
            list(APPEND libs_debug_dependencies nx_vms_json_rpc)
        endif()
        if(TARGET vms_gateway_core)
            list(APPEND libs_debug_dependencies vms_gateway_core)
        endif()
        if(TARGET nx_vms_applauncher_api)
            list(APPEND libs_debug_dependencies nx_vms_applauncher_api)
        endif()
        if(TARGET qtkeychain)
            list(APPEND libs_debug_dependencies qtkeychain)
        endif()

        set(libs_debug_file "${distribution_output_dir}/${libs_debug_distribution_name}.zip")
        nx_create_distribution_package(
            PACKAGE_NAME "libs debug"
            PACKAGE_TARGET libs_debug
            PACKAGE_GENERATION_SCRIPT_NAME generate_libs_debug_package.py
            OUTPUT_FILE ${libs_debug_file}
            CONFIG_FILES ${distribution_config_file}
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
            DEPENDENCIES ${libs_debug_dependencies}
            TARGETS ${libs_debug_dependencies}
        )
    endif()

    set(misc_debug_dependencies)
    if(TARGET applauncher)
        list(APPEND misc_debug_dependencies applauncher)
    endif()

    if(TARGET testcamera)
        list(APPEND misc_debug_dependencies testcamera)
    endif()

    set(misc_debug_file "${distribution_output_dir}/${misc_debug_distribution_name}.zip")
    nx_create_distribution_package(
        PACKAGE_NAME "misc debug"
        PACKAGE_TARGET misc_debug
        PACKAGE_GENERATION_SCRIPT_NAME generate_misc_debug_package.py
        OUTPUT_FILE ${misc_debug_file}
        CONFIG_FILES ${distribution_config_file}
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        DEPENDENCIES ${misc_debug_dependencies}
    )
endif()

nx_add_targets_to_strengthened(
    ${dmg_file} ${update_file} ${compatibility_update_file}
)

if(NOT openSourceBuild)
    if(codeSigning)
        prepare_mac_keychain(distribution_dmg_client)
    endif()

    nx_sign_update_package(distribution_dmg_client ${update_file})
    if(compatibility_update_file)
        nx_sign_update_package(distribution_dmg_client ${compatibility_update_file})
    endif()
endif()
