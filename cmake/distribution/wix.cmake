## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

include(windows_signing)
if(NOT openSourceBuild)
    include(openssl_signing)
endif()

# Windows distributions use custom qt.conf due to different directory structure.
set(qt_conf_path "${open_source_root}/vms/distribution/wix/qt.conf")
set(full_release_version "${releaseVersion.full}")
set(bin_source_dir ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})

set(customInstallerIcon "${customization_dir}/icons/windows/installer_icon.ico")
if(EXISTS "${customInstallerIcon}")
   set(installerIcon ${customInstallerIcon})
else()
   set(installerIcon "${customization_dir}/icons/all/favicon.ico")
endif()

function(nx_wix_generate_localization_file)
    message(STATUS "Generate Wix localization file")
    set(_wixDir ${open_source_root}/vms/distribution/wix)
    set(localization_file "${CMAKE_CURRENT_BINARY_DIR}/theme_${customization.defaultLanguage}.wxl")
    nx_execute_process_or_fail(
        COMMAND ${PYTHON_EXECUTABLE} ${_wixDir}/scripts/merge_wxl_files.py
            ${_wixDir}/translations/strings_${customization.defaultLanguage}.wxl
            ${_wixDir}/localization/metrics_${customization.defaultLanguage}.wxl
            --output_file ${localization_file}.tmp
    )
    execute_process(COMMAND
        ${CMAKE_COMMAND} -E copy_if_different ${localization_file}.tmp ${localization_file})

    nx_store_known_file(${localization_file})
    nx_expose_to_parent_scope(localization_file)
endfunction()
