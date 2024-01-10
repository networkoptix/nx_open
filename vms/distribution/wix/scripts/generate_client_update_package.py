#!/bin/python3

## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import os
import zipfile

import distribution_tools as tools

qt_libraries = [
    'Concurrent',
    'Core',
    'Core5Compat',
    'Gui',
    'LabsQmlModels',
    'Multimedia',
    'MultimediaQuick',
    'MultimediaWidgets',
    'Network',
    'OpenGL',
    'OpenGLWidgets',
    'Positioning',
    'PositioningQuick',
    'PrintSupport',
    'Qml',
    'QmlModels',
    'QmlWorkerScript',
    'Quick',
    'QuickControls2',
    'QuickControls2Impl',
    'QuickEffects',
    'QuickLayouts',
    'QuickShapes',
    'QuickTemplates2',
    'QuickWidgets',
    'ShaderTools',
    'Sql',
    'Svg',
    'WebChannel',
    'WebEngineCore',
    'WebEngineQuick',
    'WebEngineWidgets',
    'Widgets',
    'Xml',
]

qt_binaries = [
    'QtWebEngineProcess.exe',
]

qt_plugins = [
    'imageformats/qgif.dll',
    'imageformats/qjpeg.dll',
    'imageformats/qtiff.dll',
    'platforms/qwindows.dll',
    'tls/qopensslbackend.dll',
    'multimedia/windowsmediaplugin.dll']

nx_libraries = [
    'nx_vms_common',
    'nx_vms_client_core',
    'nx_vms_client_desktop',
    'nx_vms_utils',
    'nx_vms_applauncher_api',
    'nx_media_core',
    'nx_media',
    'nx_pathkit',
    'nx_rtp',
    'nx_codec',
    'nx_utils',
    'nx_fusion',
    'nx_reflect',
    'nx_network',
    'nx_kit',
    'nx_vms_api',
    'nx_vms_update',
    'nx_vms_rules',
    'nx_zip',
    'nx_branding',
    'nx_build_info',
    'nx_monitoring',
    'vms_gateway_core',
    'udt',
    'cudart64_110',
    'qtkeychain'
]


def create_client_update_file(config, output_file):
    customization_mobile_client_enabled = config['customization_mobile_client_enabled']

    # Directory, where icu libraries reside:
    icu_lib_directory = os.path.join(config['icu_directory'], 'bin')

    binaries_dir = config['bin_source_dir']
    current_binary_dir = config['current_binary_dir']
    qt_directory = config['qt_directory']
    client_qml_dir = config['client_qml_dir']
    client_binary_name = config['client_binary_name']
    launcher_version_name = config['launcher_version_file']
    minilauncher_binary_name = config['minilauncher_binary_name']
    client_update_files_directory = config['client_update_files_directory']
    locale_resources_directory = os.path.join(qt_directory, 'resources')
    webengine_locales_directory = os.path.join(qt_directory, 'translations', 'qtwebengine_locales')
    mobile_help_source_file_path = config['mobile_help_source_file_path']
    mobile_help_destination_file_name = config['mobile_help_destination_file_name']
    quick_start_guide_source_file_path = config['quick_start_guide_source_file_path']
    quick_start_guide_destination_file_name = config['quick_start_guide_destination_file_name']
    asan_library_name = config['asan_library_name']

    with zipfile.ZipFile(output_file, "w", zipfile.ZIP_DEFLATED) as zip:
        tools.zip_files_to(zip, tools.ffmpeg_files(binaries_dir), binaries_dir)
        tools.zip_files_to(zip, tools.openssl_files(binaries_dir), binaries_dir)
        tools.zip_files_to(zip, tools.hidapi_files(binaries_dir), binaries_dir)
        tools.zip_files_to(zip, tools.openal_files(binaries_dir), binaries_dir)
        tools.zip_files_to(zip, tools.quazip_files_to(binaries_dir), binaries_dir)
        tools.zip_files_to(zip, tools.nx_files(binaries_dir, nx_libraries), binaries_dir)
        tools.zip_files_to(zip, tools.find_all_files(client_update_files_directory),
           client_update_files_directory)
        tools.zip_files_to(zip, tools.icu_files(icu_lib_directory), icu_lib_directory)
        tools.zip_files_to(zip, tools.find_all_files(locale_resources_directory), qt_directory)
        tools.zip_files_to(zip, tools.find_all_files(webengine_locales_directory), qt_directory)

        if customization_mobile_client_enabled:
            zip.write(mobile_help_source_file_path, mobile_help_destination_file_name)

        zip.write(quick_start_guide_source_file_path, quick_start_guide_destination_file_name)

        qt_bin_dir = os.path.join(qt_directory, 'bin')
        tools.zip_files_to(zip, tools.qt_libraries(qt_bin_dir, qt_libraries), qt_bin_dir)
        for qt_binary in qt_binaries:
            zip.write(os.path.join(qt_bin_dir, qt_binary), qt_binary)
        tools.zip_files_to(zip, tools.find_all_files(client_qml_dir), client_qml_dir, 'qml')

        qt_plugins_dir = os.path.join(qt_directory, 'plugins')
        tools.zip_files_to(zip, tools.qt_plugins_files(qt_plugins_dir, qt_plugins), qt_plugins_dir)

        tools.zip_all_files(zip, config['help_directory'])
        tools.zip_all_files(zip, os.path.join(config['ucrt_directory'], 'bin'))
        tools.zip_all_files(zip, config['vcrt_directory'])
        tools.zip_all_files(zip, os.path.join(config['fonts_directory'], 'bin'))

        zip.write(os.path.join(binaries_dir, 'client_external.dat'), 'client_external.dat')
        zip.write(os.path.join(binaries_dir, 'client_core_external.dat'), 'client_core_external.dat')
        zip.write(os.path.join(binaries_dir, 'bytedance_iconpark.dat'), 'bytedance_iconpark.dat')
        translations_dir = os.path.join(binaries_dir, 'translations')
        zip.write(os.path.join(translations_dir, 'nx_vms_common.dat'), 'translations/nx_vms_common.dat')
        zip.write(os.path.join(translations_dir, 'nx_vms_rules.dat'), 'translations/nx_vms_rules.dat')
        zip.write(os.path.join(translations_dir, 'nx_vms_client_core.dat'), 'translations/nx_vms_client_core.dat')
        zip.write(os.path.join(translations_dir, 'nx_vms_client_desktop.dat'), 'translations/nx_vms_client_desktop.dat')
        if asan_library_name:
            zip.write(os.path.join(binaries_dir, asan_library_name), asan_library_name)

        zip.write(os.path.join(binaries_dir, client_binary_name), client_binary_name)
        zip.write(os.path.join(binaries_dir, 'applauncher.exe'), 'applauncher.exe')
        zip.write(os.path.join(binaries_dir, launcher_version_name), launcher_version_name)
        zip.write(os.path.join(binaries_dir, minilauncher_binary_name), minilauncher_binary_name)
        zip.write(os.path.join(current_binary_dir, 'qt.conf'), 'qt.conf')
        zip.write(os.path.join(config['distribution_output_dir'], 'build_info.txt'), 'build_info.txt')
        zip.write(os.path.join(config['distribution_output_dir'], 'build_info.json'), 'build_info.json')


def main():
    args = tools.parse_generation_scripts_arguments()
    create_client_update_file(args.config, args.output_file)


if __name__ == '__main__':
    main()
