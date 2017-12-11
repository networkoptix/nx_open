set(project.build.directory "${CMAKE_CURRENT_BINARY_DIR}")
set(bin_resources "${CMAKE_CURRENT_SOURCE_DIR}/maven/bin-resources")
set(filter_resources "${CMAKE_CURRENT_SOURCE_DIR}/maven/filter-resources")
set(ClientHelpSourceDir "${bin_source_dir}/help")
set(VoxSourceDir "${bin_source_dir}/vox")
set(ClientQmlDir "${CMAKE_CURRENT_BINARY_DIR}/clientqml")
set(ClientBgSourceDir "${CMAKE_CURRENT_BINARY_DIR}/Backgrounds")
set(ClientFontsDir "${bin_source_dir}/fonts")

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

add_custom_target(wixsetup ALL
    DEPENDS
        desktop_client
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
        generate_update_packages
    COMMENT "Creating installer..."
    # TODO move python to cmake
    COMMAND ${CMAKE_COMMAND} -E copy
        "${bin_source_dir}/minilauncher.exe" "${bin_source_dir}/${minilauncher.binary.name}"
    COMMAND ${CMAKE_COMMAND} -E copy
        "${bin_source_dir}/desktop_client.exe" "${bin_source_dir}/${client.binary.name}"
    COMMAND ${PYTHON_EXECUTABLE} wix-gen.py
    COMMAND ${PYTHON_EXECUTABLE} wix-compile.py
)

add_custom_target(generate_update_packages ALL
    COMMENT "Preparing update and debug packages ..."
    DEPENDS wixsetup
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/wixsetup
    COMMAND ${PYTHON_EXECUTABLE} generate_update_packages.py 
        "--binaries" ${bin_source_dir}
        "--qt" ${QT_DIR}
        "--redist" ${VC14RedistPath}
        "--vox" ${festival_vox_directory}
        "--fonts" ${fonts_directory}
        "--client-qml" ${ClientQmlDir}
        "--client-update" ${client_update_distribution_name}
        "--client-binary-name" ${product.name}
        "--launcher-version" ${launcher.version.file}
        "--minilauncher-binary-name" ${minilauncher.binary.name}
        "--output" ${distribution_output_dir}
)
