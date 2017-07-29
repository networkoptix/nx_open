set(project.build.directory "${CMAKE_CURRENT_BINARY_DIR}")
set(bin_resources "${CMAKE_CURRENT_SOURCE_DIR}/maven/bin-resources")
set(target_bin_path "${CMAKE_BINARY_DIR}/${config}/bin")
set(filter_resources "${CMAKE_CURRENT_SOURCE_DIR}/maven/filter-resources")
set(ClientHelpSourceDir "${target_bin_path}/help")
set(ClientVoxSourceDir "${target_bin_path}/vox")
set(ClientQmlDir "${CMAKE_CURRENT_BINARY_DIR}/clientqml")
set(ClientBgSourceDir "${CMAKE_CURRENT_BINARY_DIR}/Backgrounds")
set(ClientFontsDir "${target_bin_path}/fonts")
set(VC14RedistPath "$ENV{environment}/packages/${rdep_target}/vcredist-2015")
#set(NxtoolQuickControlsDir ${libdir}\bin\${build.configuration}\qml\QtQuick\Controls)

ExternalProject_Add(
    customactions
    SOURCE_DIR "${bin_resources}/CustomActions"
    CMAKE_ARGS
        -DCMAKE_INSTALL_PREFIX=${CMAKE_CURRENT_BINARY_DIR}/CustomActions
        -DCONFIG_DIRECTORY=${CMAKE_CURRENT_BINARY_DIR}
        -DROOT_DIR=${CMAKE_SOURCE_DIR}
)

nx_copy_bin_resources(${bin_resources} "${CMAKE_CURRENT_BINARY_DIR}")
nx_copy_bin_resources(${customization_dir}/wixsetup "${CMAKE_CURRENT_BINARY_DIR}")
nx_configure_directory(${filter_resources} "${CMAKE_CURRENT_BINARY_DIR}")

file(WRITE ${bin_source_dir}/launcher.version "${releaseVersion.full}")

add_custom_target(wixsetup ALL
    DEPENDS desktop_client applauncher traytool mediaserver minilauncher customactions
    COMMENT "Creating installer..."
    COMMAND ${PYTHON_EXECUTABLE} wix-gen.py
    COMMAND ${PYTHON_EXECUTABLE} wix-compile.py
)