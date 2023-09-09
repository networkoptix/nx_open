## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

include(${open_source_root}/cmake/distribution/common.cmake)
include(${open_source_root}/cmake/distribution/wix.cmake)
include(${open_source_root}/cmake/wix_utils.cmake)

# TODO: Move update generation to the Client project.
set(client_qml_directory "${CMAKE_CURRENT_BINARY_DIR}/desktop_client/qml")

set(client_launch_target
    "[ProgramFiles64Folder]/${windowsInstallPath}/${customization.vmsName}/Client/${full_release_version}/${minilauncher.binary.name}"
    CACHE INTERNAL "" FORCE)

set(modules_output_path ${CMAKE_CURRENT_BINARY_DIR}/msm)

set(wix_theme_file "${CMAKE_CURRENT_BINARY_DIR}/theme.xml" CACHE INTERNAL "" FORCE)
set(common_wixobjects_path ${CMAKE_CURRENT_BINARY_DIR}/common CACHE INTERNAL "" FORCE)

add_subdirectory(custom_actions)

nx_configure_directory(${CMAKE_CURRENT_SOURCE_DIR}/resources.in ${CMAKE_CURRENT_BINARY_DIR})

# This folder is configured, and is required for update packages only.
set(client_update_files_directory "${CMAKE_CURRENT_BINARY_DIR}/client_update_package")

nx_generate_package_json(${client_update_files_directory}/package.json
    COMPONENT client
    VARIANTS "{ \"minimumVersion\": \"6.3\" }") #< Corresponds to Windows 8.1.

# Copy qt.conf for the Client update package generation script.
file(COPY "${qt_conf_path}" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")
nx_store_known_file(${CMAKE_CURRENT_BINARY_DIR}/qt.conf)

file(WRITE ${bin_source_dir}/${launcher.version.file} "${releaseVersion.full}")
nx_store_known_file(${bin_source_dir}/${launcher.version.file})

set(wix_distribution_config_source_file "${CMAKE_CURRENT_SOURCE_DIR}/config.yaml"
    CACHE INTERNAL "" FORCE)
set(distribution_config_file "${CMAKE_CURRENT_BINARY_DIR}/config.yaml")
nx_configure_file("${wix_distribution_config_source_file}" "${distribution_config_file}")

if(withMediaServer OR withDesktopClient)
    add_subdirectory(common)
endif()

if(withDesktopClient)
    add_subdirectory(desktop_client)

    set(client_update_file "${distribution_output_dir}/${client_update_distribution_name}.zip")
    nx_create_distribution_package(
        PACKAGE_NAME "client update"
        PACKAGE_TARGET client_update
        PACKAGE_GENERATION_SCRIPT_NAME generate_client_update_package.py
        OUTPUT_FILE ${client_update_file}
        CONFIG_FILES ${distribution_config_file}
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/scripts"
        DEPENDENCIES applauncher minilauncher desktop_client)
    if(NOT openSourceBuild)
        nx_sign_update_package(generate_client_update_package ${client_update_file})
    endif()
endif()
