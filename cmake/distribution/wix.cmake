## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

include(${PROJECT_SOURCE_DIR}/cmake/windows_signing.cmake)
if(NOT openSourceBuild)
    include(openssl_signing)
endif()

# Windows distributions use custom qt.conf due to different directory structure.
set(qt_conf_path "${nx_vms_distribution_dir}/wix/qt.conf")
set(localization_file "${nx_vms_distribution_dir}/wix/localization/theme_${installerLanguage}.wxl")
set(full_release_version "${release.version}.${buildNumber}")

set(customInstallerIcon "${customization.dir}/icons/windows/installer_icon.ico")
if(EXISTS "${customInstallerIcon}")
   set(installerIcon ${customInstallerIcon})
else()
   set(installerIcon "${customization.dir}/icons/all/favicon.ico")
endif()
