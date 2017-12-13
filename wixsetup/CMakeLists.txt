set(project.build.directory "${CMAKE_CURRENT_BINARY_DIR}")
set(bin_resources "${CMAKE_CURRENT_SOURCE_DIR}/maven/bin-resources")
set(filter_resources "${CMAKE_CURRENT_SOURCE_DIR}/maven/filter-resources")
set(ClientHelpSourceDir "${bin_source_dir}/help")
set(VoxSourceDir "${bin_source_dir}/vox")
set(client_qml_dir "${CMAKE_CURRENT_BINARY_DIR}/client_qml")
set(ClientBgSourceDir "${CMAKE_CURRENT_BINARY_DIR}/Backgrounds")
set(ClientFontsDir "${bin_source_dir}/fonts")
set(VC14RedistPath "${vcredist_directory}")

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

set(desktop_client_executable "${bin_source_dir}/desktop_client.exe")
# This line is version-dependent. In vms-4.0 path is 'static-resources/qml/'
set(desktop_client_qml_root "${CMAKE_SOURCE_DIR}/client/nx_client_desktop/static-resources/src/qml/")

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

add_custom_target(wixsetup ALL
    DEPENDS
        collect_qml_dependencies
        applauncher
        traytool
        mediaserver
        minilauncher
        customactions
        testcamera
        genericrtspplugin
        mjpg_link
        generic_multicast_plugin
        hanwha_metadata_plugin
    COMMENT "Creating installer..."
    # TODO move python to cmake
    COMMAND ${CMAKE_COMMAND} -E copy
        "${bin_source_dir}/minilauncher.exe" "${bin_source_dir}/${minilauncher.binary.name}"
    COMMAND ${CMAKE_COMMAND} -E copy
        "${desktop_client_executable}" "${bin_source_dir}/${client.binary.name}"
    COMMAND ${PYTHON_EXECUTABLE} wix-gen.py
    COMMAND ${PYTHON_EXECUTABLE} wix-compile.py
)

add_custom_target(generate_update_packages ALL
    COMMENT "Preparing update and debug packages ..."
    DEPENDS
        collect_qml_dependencies
        applauncher
        minilauncher
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/wixsetup/scripts
    COMMAND ${PYTHON_EXECUTABLE} generate_update_packages.py
        "--config" ${config_file}
        "--output" ${distribution_output_dir}
)
