## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

include(wix_utils)

add_subdirectory(modules)

set(common_wix_sources_path ${CMAKE_CURRENT_SOURCE_DIR}/../common)
set(backgrounds_directory "${vms_distribution_common_dir}/desktop_client/backgrounds")
set(client_registry_path "Software\\${customization.companyName}\\${desktopClientInternalName}")
set(client_configs_path "$ENV{LOCALAPPDATA}\\${customization.companyName}\\${desktopClientInternalName}")
set(unsigned_msi_filename ${CMAKE_CURRENT_BINARY_DIR}/client-unsigned.msi)
set(signed_msi_filename ${CMAKE_CURRENT_BINARY_DIR}/client-signed.msi)
set(signed_distribution_path "${distribution_output_dir}/${client_distribution_name}.exe")


nx_wix_heat(${CMAKE_CURRENT_BINARY_DIR}/client_webengine_resources.wixobj
    SOURCE_DIR ${QT_DIR}/resources
    TARGET_DIR_ALIAS WebEngineResourcesDirectoryReference
    COMPONENT_GROUP WebEngineResourcesComponentGroup)

nx_wix_heat(${CMAKE_CURRENT_BINARY_DIR}/client_webengine_locales.wixobj
    SOURCE_DIR ${QT_DIR}/translations/qtwebengine_locales
    TARGET_DIR_ALIAS WebEngineLocalesDirectoryReference
    COMPONENT_GROUP WebEngineLocalesComponentGroup)

nx_wix_heat(${CMAKE_CURRENT_BINARY_DIR}/client_fonts.wixobj
    SOURCE_DIR ${CONAN_ROBOTO-FONTS_ROOT}/bin/fonts
    TARGET_DIR_ALIAS FontsDirectoryReference
    COMPONENT_GROUP FontsComponentGroup)

nx_wix_heat(${CMAKE_CURRENT_BINARY_DIR}/client_backgrounds.wixobj
    SOURCE_DIR ${backgrounds_directory}
    TARGET_DIR_ALIAS BackgroundsDirectoryReference
    COMPONENT_GROUP BackgroundsComponentGroup)

nx_wix_candle(files_associations)
nx_wix_candle(client_artifacts)
nx_wix_candle(client_nx_libraries)
nx_wix_candle(client_feature EXTENSIONS WixFirewallExtension)
nx_wix_candle(client_product)

nx_wix_light(${unsigned_msi_filename}
    LOCALIZATION_FILE ${localization_file}
    SOURCE_FILES
        ${CMAKE_CURRENT_BINARY_DIR}/client_product.wixobj
        ${CMAKE_CURRENT_BINARY_DIR}/client_feature.wixobj
        ${CMAKE_CURRENT_BINARY_DIR}/client_nx_libraries.wixobj
        ${CMAKE_CURRENT_BINARY_DIR}/client_artifacts.wixobj
        ${CMAKE_CURRENT_BINARY_DIR}/files_associations.wixobj
        ${CMAKE_CURRENT_BINARY_DIR}/client_backgrounds.wixobj
        ${CMAKE_CURRENT_BINARY_DIR}/client_fonts.wixobj
        ${CMAKE_CURRENT_BINARY_DIR}/client_webengine_resources.wixobj
        ${CMAKE_CURRENT_BINARY_DIR}/client_webengine_locales.wixobj
        ${common_wixobjects_path}/common_nx_libraries.wixobj
        ${common_wixobjects_path}/common_qt_libraries.wixobj
        ${common_wixobjects_path}/common_qt_platforms.wixobj
        ${common_wixobjects_path}/common_translations.wixobj
        ${common_wixobjects_path}/ffmpeg_libraries.wixobj
        ${common_wixobjects_path}/icu_libraries.wixobj
        ${common_wixobjects_path}/openssl_libraries.wixobj
        ${common_wixobjects_path}/ucrt_libraries.wixobj
        ${common_wixobjects_path}/vcrt_libraries.wixobj
        ${common_wixobjects_path}/asan_libraries.wixobj
    EXTENSIONS
        WixUtilExtension
        WixFirewallExtension
    DEPENDS
        ${modules_output_path}/client_help.msm
        ${modules_output_path}/client_qml.msm
        desktop_client
        applauncher
        minilauncher
        wix_custom_actions
        distribution_wix_common_resources
        distribution_wix_client_qml_module
        distribution_wix_client_help_module
)

nx_add_targets_to_strengthened(${unsigned_msi_filename})

if(codeSigning)
    set(unsigned_distribution_path
        "${CMAKE_CURRENT_BINARY_DIR}/client-unsigned.exe")
    set(client_msi_filename ${signed_msi_filename})
    set(client_package_wixobject_filename
        ${CMAKE_CURRENT_BINARY_DIR}/client_signed_package.wixobj CACHE INTERNAL "" FORCE)
    set(msi_filename ${signed_msi_filename})
    nx_wix_sign_msi(${signed_msi_filename} SOURCE_FILE ${unsigned_msi_filename})
else()
    set(unsigned_distribution_path
        "${distribution_output_dir}/${client_distribution_name}-unsigned.exe")
    set(client_msi_filename ${unsigned_msi_filename})
    set(client_package_wixobject_filename
        ${CMAKE_CURRENT_BINARY_DIR}/client_unsigned_package.wixobj CACHE INTERNAL "" FORCE)
    set(msi_filename ${unsigned_msi_filename})
endif()

nx_wix_candle_ext(${client_package_wixobject_filename}
    SOURCE_FILE ${CMAKE_CURRENT_SOURCE_DIR}/client_package.wxs
    EXTENSIONS
        WixBalExtension
        WixUtilExtension)

add_custom_target(distribution_wix_client_msi ALL
    COMMENT "[Wix] Building client msi"
    DEPENDS
        ${msi_filename}
        ${client_package_wixobject_filename}
)
set_target_properties(distribution_wix_client_msi PROPERTIES FOLDER distribution)

nx_wix_candle(client_bundle
    EXTENSIONS
        WixBalExtension
        WixUtilExtension)

# Befor lighting the installer we must ensure corresponding msi package is already built.
nx_wix_light(${unsigned_distribution_path}
    LOCALIZATION_FILE ${localization_file}
    SOURCE_FILES
        ${client_package_wixobject_filename}
        ${CMAKE_CURRENT_BINARY_DIR}/client_bundle.wixobj
        ${common_wixobjects_path}/check_architecture.wixobj
    EXTENSIONS
        WixBalExtension
        WixUtilExtension
    DEPENDS
        ${msi_filename}
        distribution_wix_common_resources
)

if(codeSigning)
    nx_wix_add_signed_exe_target(distribution_wix_client_signed_exe
        SOURCE_FILE ${unsigned_distribution_path}
        TARGET_FILE ${signed_distribution_path}
    )
else()
    add_custom_target(distribution_wix_client ALL
        COMMENT "[Wix] Building client distribution"
        DEPENDS
            ${unsigned_distribution_path}
    )
    set_target_properties(distribution_wix_client PROPERTIES FOLDER distribution)
endif()
