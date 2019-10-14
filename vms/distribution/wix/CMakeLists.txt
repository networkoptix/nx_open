option(addQtPdb
    "Whether add Qt PDB files to debug libs"
    OFF)

# These variables must be defined first as they are going to config file.
set(project.build.directory "${CMAKE_CURRENT_BINARY_DIR}")
set(ClientHelpSourceDir "${help_directory}/bin/help")
set(VoxSourceDir "${bin_source_dir}/vox")
set(client_qml_dir "${CMAKE_CURRENT_BINARY_DIR}/client_qml")
set(nxtool_qml_dir "${CMAKE_CURRENT_BINARY_DIR}/nxtool_qml")
set(ClientBgSourceDir "${CMAKE_CURRENT_BINARY_DIR}/Backgrounds")
set(ClientFontsDir "${bin_source_dir}/fonts")

# These folders are configured, and are required for update packages only.
set(client_update_files_directory "${CMAKE_CURRENT_BINARY_DIR}/client")
set(server_update_files_directory "${CMAKE_CURRENT_BINARY_DIR}/server")

set(productLogo "${customization.dir}/icons/all/favicon.ico")
set(customInstallerIcon "${customization.dir}/icons/windows/installer_icon.ico")

if(EXISTS "${customInstallerIcon}")
   set(installerIcon ${customInstallerIcon})
else()
   set(installerIcon ${productLogo})
endif()

include(wix_utils)

file(TO_CMAKE_PATH ${wix_directory} wix_directory)

add_subdirectory(custom_actions)

nx_copy_bin_resources(${CMAKE_CURRENT_SOURCE_DIR}/resources ${CMAKE_CURRENT_BINARY_DIR})
nx_configure_directory(${CMAKE_CURRENT_SOURCE_DIR}/resources.in ${CMAKE_CURRENT_BINARY_DIR})

nx_generate_package_json(${client_update_files_directory}/package.json
    COMPONENT client)
nx_generate_package_json(${server_update_files_directory}/package.json
    INSTALL_SCRIPT "install.bat")

file(WRITE ${bin_source_dir}/launcher.version "${releaseVersion.full}")
file(COPY "${CMAKE_SOURCE_DIR}/vms/libs/common/maven/bin-resources/resources/qt/etc/qt.conf"
    DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")

set(config_file "${CMAKE_CURRENT_BINARY_DIR}/config.yaml")
nx_configure_file("${CMAKE_CURRENT_SOURCE_DIR}/config.yaml" "${config_file}")

if (withMediaServer OR withDesktopClient)

    set(qt_webengine_resources_dir ${qt.dir}/resources)
    set(webengine_resources_file ${CMAKE_CURRENT_BINARY_DIR}/webengine_resources.wxs)

    nx_wix_heat(${webengine_resources_file}
        SOURCE_DIR ${qt_webengine_resources_dir}
        SOURCE_DIR_ALIAS var.qt_webengine_resources_dir
        TARGET_DIR_ALIAS resources
        COMPONENT_GROUP webengine_resources)

    set(distribution_wix_client_msi_webengine_resources_obj
        ${CMAKE_CURRENT_BINARY_DIR}/obj/client-msi/webengine_resources.wixobj)
    add_custom_command(
        COMMENT "[Wix] Candle webengine resources for msi"
        MAIN_DEPENDENCY ${webengine_resources_file}
        COMMAND ${candle_executable}
            -arch x64
            -nologo
            -dqt_webengine_resources_dir=${qt_webengine_resources_dir}
            -out ${distribution_wix_client_msi_webengine_resources_obj}
            ${webengine_resources_file}
        OUTPUT ${distribution_wix_client_msi_webengine_resources_obj}
        VERBATIM
    )

    set(distribution_wix_client_strip_webengine_resources_obj
        ${CMAKE_CURRENT_BINARY_DIR}/obj/client-strip/webengine_resources.wixobj)
    add_custom_command(
        COMMENT "[Wix] Candle webengine resources for exe"
        DEPENDS ${webengine_resources_file}
        COMMAND ${candle_executable}
            -arch x64
            -nologo
            -dqt_webengine_resources_dir=${qt_webengine_resources_dir}
            -out ${distribution_wix_client_strip_webengine_resources_obj}
            ${webengine_resources_file}
        OUTPUT ${distribution_wix_client_strip_webengine_resources_obj}
        VERBATIM
    )

    add_custom_target(distribution_wix ALL
        DEPENDS
            wix_custom_actions
            ${distribution_wix_client_strip_webengine_resources_obj}
            ${distribution_wix_client_msi_webengine_resources_obj}
        COMMENT "Creating installer..."
        COMMAND ${PYTHON_EXECUTABLE} wix-gen.py
        COMMAND ${PYTHON_EXECUTABLE} wix-compile.py
            "--config" ${config_file}
            "--output" ${distribution_output_dir}
    )
    set_target_properties(distribution_wix PROPERTIES FOLDER distribution)
endif()


if(withTestCamera)
    add_dependencies(distribution_wix testcamera)

    add_custom_target(generate_testcamera_package ALL
        COMMENT "Preparing testcamera package..."
        DEPENDS
            testcamera
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/scripts
        COMMAND ${PYTHON_EXECUTABLE} generate_testcamera_package.py
            "--config" ${config_file}
            "--output" ${distribution_output_dir}
    )
endif()


if(withNxTool)
    set(nxtool_qml_root "${CMAKE_SOURCE_DIR}/vms/nxtool/static-resources/src/qml/")
    set(nxtool_executable "${bin_source_dir}/nxtool.exe")

    add_custom_target(collect_nxtool_qml_dependencies
        DEPENDS
            nxtool
            wix_custom_actions
        COMMENT "Collecting NxTool QML libraries..."
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/build_utils/python
        COMMAND ${PYTHON_EXECUTABLE} qmldeploy.py
            "--qml-root" ${nxtool_qml_root}
            "--qt-root" ${qt.dir}
            "--output" ${nxtool_qml_dir}
            "--skip-styles"
        )

    add_custom_target(create_nxtool_installer ALL
        COMMENT "Generating NxTool installer..."
        DEPENDS
            collect_nxtool_qml_dependencies
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/scripts
        COMMAND ${PYTHON_EXECUTABLE} create_nxtool_installer.py
            "--config" ${config_file}
            "--qml-dir" ${nxtool_qml_dir}
            "--sources-dir" "${CMAKE_CURRENT_BINARY_DIR}"
            "--output" ${distribution_output_dir}
    )
endif()

if(withDesktopClient)
    set(desktop_client_qml_root "${CMAKE_SOURCE_DIR}/vms/client/nx_vms_client_desktop/static-resources/qml/")
    set(desktop_client_executable "${bin_source_dir}/nx_vms_client_desktop.dll")

    add_custom_target(collect_qml_dependencies
        DEPENDS
            desktop_client
        COMMENT "Collecting QML libraries..."
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/build_utils/python
        COMMAND ${PYTHON_EXECUTABLE} qmldeploy.py
            "--qml-root" ${desktop_client_qml_root}
            "--qt-root" ${qt.dir}
            "--output" ${client_qml_dir}
            "--prefer-compiled"
            "--skip-styles"
            "--clean"
        )
    add_dependencies(distribution_wix collect_qml_dependencies applauncher minilauncher)

    add_custom_target(generate_client_update_package ALL
    COMMENT "Preparing client update package..."
    DEPENDS
        collect_qml_dependencies
        applauncher
        minilauncher
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/scripts
    COMMAND ${PYTHON_EXECUTABLE} generate_client_update_package.py
        "--config" ${config_file}
        "--output" ${distribution_output_dir}
    )

    add_custom_target(generate_client_debug_package ALL
        COMMENT "Preparing client debug package..."
        DEPENDS
            desktop_client
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/scripts
        COMMAND ${PYTHON_EXECUTABLE} generate_client_debug_package.py
            "--config" ${config_file}
            "--output" ${distribution_output_dir}
    )
endif()

if(withMediaServer)
    add_dependencies(distribution_wix
        mediaserver
        server_plugins
    )

    if(withTrayTool)
        add_dependencies(distribution_wix traytool)
    endif()

    add_custom_target(generate_server_update_package ALL
        COMMENT "Preparing mediaserver update package..."
        DEPENDS
            distribution_wix
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/scripts
        COMMAND ${PYTHON_EXECUTABLE} generate_server_update_package.py
            "--config" ${config_file}
            "--output" ${distribution_output_dir}
    )
endif()


if(withDesktopClient OR withMediaServer OR withTestCamera)
    add_custom_target(generate_libs_debug_package ALL
        COMMENT "Preparing libs debug package..."
        DEPENDS
            nx_network
            nx_utils
            nx_vms_utils
            nx_vms_api
            nx_kit
            udt
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/scripts
        COMMAND ${PYTHON_EXECUTABLE} generate_libs_debug_package.py
            "--config" ${config_file}
            "--output" ${distribution_output_dir}
    )
    if(TARGET nx_sql)
         add_dependencies(generate_libs_debug_package nx_sql)
     endif()
     if(TARGET nx_vms_api)
         add_dependencies(generate_libs_debug_package nx_vms_api)
     endif()
     if(TARGET nx_vms_utils)
         add_dependencies(generate_libs_debug_package nx_vms_utils)
     endif()
     if(TARGET vms_gateway_core)
         add_dependencies(generate_libs_debug_package vms_gateway_core)
     endif()
     if(TARGET nx_vms_applauncher_api)
         add_dependencies(generate_libs_debug_package nx_vms_applauncher_api)
     endif()
     if(TARGET qtkeychain)
         add_dependencies(generate_libs_debug_package qtkeychain)
     endif()
endif()

if(withMediaServer)
    add_custom_target(generate_mediaserver_debug_package ALL
        COMMENT "Preparing mediaserver debug package..."
        DEPENDS
            mediaserver
            server_plugins

        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/scripts
        COMMAND ${PYTHON_EXECUTABLE} generate_mediaserver_debug_package.py
            "--config" ${config_file}
            "--output" ${distribution_output_dir}
    )
endif()

if(withClouds)
    add_custom_target(generate_cloud_debug_package ALL
        COMMENT "Preparing cloud debug package..."
        DEPENDS
            cloud_connect_test_util
            cloud_db
            connection_mediator
            vms_gateway
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/scripts
        COMMAND ${PYTHON_EXECUTABLE} generate_cloud_debug_package.py
            "--config" ${config_file}
            "--output" ${distribution_output_dir}
    )
endif()

if(addQtPdb)
    add_custom_target(generate_qt_debug_package ALL
        COMMENT "Preparing Qt debug package..."
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/scripts
        COMMAND ${PYTHON_EXECUTABLE} generate_qt_debug_package.py
            "--config" ${config_file}
            "--output" ${distribution_output_dir}
    )
endif()

add_custom_target(generate_misc_debug_package ALL
    COMMENT "Preparing misc debug package..."
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/scripts
    COMMAND ${PYTHON_EXECUTABLE} generate_misc_debug_package.py
        "--config" ${config_file}
        "--output" ${distribution_output_dir}
)

if(TARGET applauncher)
    add_dependencies(generate_misc_debug_package applauncher)
endif()

if(TARGET minilauncher)
    add_dependencies(generate_misc_debug_package minilauncher)
endif()

if(TARGET traytool)
    add_dependencies(generate_misc_debug_package traytool)
endif()

if(TARGET testcamera)
    add_dependencies(generate_misc_debug_package testcamera)
endif()

if(TARGET media_db_util)
    add_dependencies(generate_misc_debug_package media_db_util)
endif()
