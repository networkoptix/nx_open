# These variables must be defined first as they are going to config file
set(project.build.directory "${CMAKE_CURRENT_BINARY_DIR}")
set(bin_resources "${CMAKE_CURRENT_SOURCE_DIR}/maven/bin-resources")
set(filter_resources "${CMAKE_CURRENT_SOURCE_DIR}/maven/filter-resources")
set(ClientHelpSourceDir "${bin_source_dir}/help")
set(VoxSourceDir "${bin_source_dir}/vox")
set(client_qml_dir "${CMAKE_CURRENT_BINARY_DIR}/client_qml")
set(nxtool_qml_dir "${CMAKE_CURRENT_BINARY_DIR}/nxtool_qml")
set(ClientBgSourceDir "${CMAKE_CURRENT_BINARY_DIR}/Backgrounds")
set(ClientFontsDir "${bin_source_dir}/fonts")
set(VC14RedistPath "${vcredist_directory}")

# These folders are filtered from filter-resources and required for update packages only
set(client_update_files_directory "${CMAKE_CURRENT_BINARY_DIR}/client")
set(server_update_files_directory "${CMAKE_CURRENT_BINARY_DIR}/server")

file(TO_CMAKE_PATH ${wix_directory} wix_directory)
file(TO_CMAKE_PATH ${signtool_directory} signtool_directory)
file(TO_CMAKE_PATH ${certificates_path} certificates_path)
file(TO_CMAKE_PATH ${VC14RedistPath} VC14RedistPath)

ExternalProject_Add(
    customactions
    SOURCE_DIR "${bin_resources}/CustomActions"
    CMAKE_ARGS
        -DCMAKE_INSTALL_PREFIX=${CMAKE_CURRENT_BINARY_DIR}/CustomActions
        -DCONFIG_DIRECTORY=${CMAKE_CURRENT_BINARY_DIR}
        -DROOT_DIR=${CMAKE_SOURCE_DIR}
        -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
        -DwixDirectory=${wix_directory}
)

nx_copy_bin_resources(${bin_resources} "${CMAKE_CURRENT_BINARY_DIR}")
nx_copy_bin_resources("${customization_dir}/wixsetup" "${CMAKE_CURRENT_BINARY_DIR}")
nx_configure_directory(${filter_resources} "${CMAKE_CURRENT_BINARY_DIR}")

file(WRITE ${bin_source_dir}/launcher.version "${releaseVersion.full}")

set(config_file "${CMAKE_CURRENT_BINARY_DIR}/config.yaml")
nx_configure_file("${CMAKE_CURRENT_SOURCE_DIR}/config.yaml" "${config_file}")

if (withMediaServer OR withDesktopClient)
    add_custom_target(wixsetup ALL
        DEPENDS
            customactions
        COMMENT "Creating installer..."
        COMMAND ${PYTHON_EXECUTABLE} wix-gen.py
        COMMAND ${PYTHON_EXECUTABLE} wix-compile.py
    )
endif()


if(withTestCamera)
    add_dependencies(wixsetup testcamera)

    add_custom_target(generate_testcamera_package ALL
        COMMENT "Preparing testcamera package..."
        DEPENDS
            testcamera
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/wixsetup/scripts
        COMMAND ${PYTHON_EXECUTABLE} generate_testcamera_package.py
            "--config" ${config_file}
            "--output" ${distribution_output_dir}
    )
endif()


if(withNxTool)
    set(nxtool_qml_root "${CMAKE_SOURCE_DIR}/nxtool/static-resources/src/qml/")
    set(nxtool_executable "${bin_source_dir}/nxtool.exe")

    add_custom_target(collect_nxtool_qml_dependencies ALL
        DEPENDS
            nxtool
        COMMENT "Collecting NxTool QML libraries..."
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/wixsetup/scripts
        COMMAND ${PYTHON_EXECUTABLE} collect_qml_dependencies.py
            "--config" ${config_file}
            "--source" ${nxtool_executable}
            "--qml-root" ${nxtool_qml_root}
            "--output" ${nxtool_qml_dir}
        )

    add_custom_target(create_nxtool_installer ALL
        COMMENT "Generating NxTool installer..."
        DEPENDS
            collect_nxtool_qml_dependencies
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/wixsetup/scripts
        COMMAND ${PYTHON_EXECUTABLE} create_nxtool_installer.py
            "--config" ${config_file}
            "--qml-dir" ${nxtool_qml_dir}
            "--sources-dir" "${CMAKE_CURRENT_BINARY_DIR}"
            "--output" ${distribution_output_dir}
    )
endif()

if(withDesktopClient)
    # This line is version-dependent. In vms-4.0 path is 'static-resources/qml/'
    set(desktop_client_qml_root "${CMAKE_SOURCE_DIR}/client/nx_client_desktop/static-resources/src/qml/")
    set(desktop_client_executable "${bin_source_dir}/desktop_client.exe")

    add_custom_target(collect_qml_dependencies ALL
        DEPENDS
            desktop_client
        COMMENT "Collecting QML libraries..."
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/wixsetup/scripts
        COMMAND ${PYTHON_EXECUTABLE} collect_qml_dependencies.py
            "--config" ${config_file}
            "--source" ${desktop_client_executable}
            "--qml-root" ${desktop_client_qml_root}
            "--output" ${client_qml_dir}
        )
    add_dependencies(wixsetup collect_qml_dependencies applauncher minilauncher)

    add_custom_target(copy_client_binaries ALL
    COMMENT "Copying client binaries..."
    DEPENDS
        desktop_client
        minilauncher
    COMMAND ${CMAKE_COMMAND} -E copy
        "${bin_source_dir}/minilauncher.exe" "${bin_source_dir}/${minilauncher.binary.name}"
    COMMAND ${CMAKE_COMMAND} -E copy
        "${desktop_client_executable}" "${bin_source_dir}/${client.binary.name}"
    )
    add_dependencies(wixsetup copy_client_binaries)

    add_custom_target(generate_client_update_package ALL
    COMMENT "Preparing client update package..."
    DEPENDS
        collect_qml_dependencies
        applauncher
        minilauncher
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/wixsetup/scripts
    COMMAND ${PYTHON_EXECUTABLE} generate_client_update_package.py
        "--config" ${config_file}
        "--output" ${distribution_output_dir}
    )

    add_custom_target(generate_client_debug_package ALL
        COMMENT "Preparing client debug package..."
        DEPENDS
            desktop_client
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/wixsetup/scripts
        COMMAND ${PYTHON_EXECUTABLE} generate_client_debug_package.py
            "--config" ${config_file}
            "--output" ${distribution_output_dir}
    )
endif()

if(withMediaServer)
    add_dependencies(wixsetup
        mediaserver
        genericrtspplugin
        mjpg_link
        generic_multicast_plugin)

    if(TARGET hanwha_metadata_plugin)
        add_dependencies(wixsetup hanwha_metadata_plugin)
    endif()

    if(withTrayTool)
        add_dependencies(wixsetup traytool)
    endif()

    add_custom_target(generate_server_update_package ALL
        COMMENT "Preparing mediaserver update package..."
        DEPENDS
            wixsetup
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/wixsetup/scripts
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
            udt
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/wixsetup/scripts
        COMMAND ${PYTHON_EXECUTABLE} generate_libs_debug_package.py
            "--config" ${config_file}
            "--output" ${distribution_output_dir}
    )
endif()

if(withMediaServer)
    add_custom_target(generate_mediaserver_debug_package ALL
        COMMENT "Preparing mediaserver debug package..."
        DEPENDS
            mediaserver
            genericrtspplugin
            generic_multicast_plugin
            image_library_plugin
            mjpg_link
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/wixsetup/scripts
        COMMAND ${PYTHON_EXECUTABLE} generate_mediaserver_debug_package.py
            "--config" ${config_file}
            "--output" ${distribution_output_dir}
    )
    if(TARGET hanwha_metadata_plugin)
        add_dependencies(generate_mediaserver_debug_package hanwha_metadata_plugin)
    endif()
endif()

if(withClouds)
    add_custom_target(generate_cloud_debug_package ALL
        COMMENT "Preparing cloud debug package..."
        DEPENDS
            cloud_connect_test_util
            cloud_db
            connection_mediator
            vms_gateway
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/wixsetup/scripts
        COMMAND ${PYTHON_EXECUTABLE} generate_cloud_debug_package.py
            "--config" ${config_file}
            "--output" ${distribution_output_dir}
    )
endif()

add_custom_target(generate_misc_debug_package ALL
    COMMENT "Preparing misc debug package..."
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/wixsetup/scripts
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
