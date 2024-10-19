#!/bin/python3

## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import zipfile

from pathlib import Path

import distribution_tools as tools

qt_libraries = [
    'Concurrent',
    'Core',
    'Core5Compat',
    'Gui',
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
    'QmlMeta',
    'QmlModels',
    'QmlWorkerScript',
    'Quick',
    'QuickControls2',
    'QuickControls2Basic',
    'QuickControls2BasicStyleImpl',
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
    'WebChannelQuick',
    'WebEngineCore',
    'WebEngineQuick',
    'WebEngineWidgets',
    'WebView',
    'WebViewQuick',
    'Widgets',
    'Xml',
]

qt_binaries = [
    'QtWebEngineProcess.exe',
]

qt_plugins = [
    'imageformats/qgif.dll',
    'imageformats/qjpeg.dll',
    'imageformats/qsvg.dll',
    'imageformats/qtiff.dll',
    'platforms/qwindows.dll',
    'tls/qopensslbackend.dll',
    'webview/qtwebview_webengine.dll',
    'multimedia/windowsmediaplugin.dll']

nx_libraries = [
    'cudart64_12',
    'libvpl',
    'nx_branding',
    'nx_build_info',
    'nx_codec',
    'nx_fusion',
    'nx_json_rpc',
    'nx_kit',
    'nx_media',
    'nx_media_core',
    'nx_network',
    'nx_network_rest',
    'nx_reflect',
    'nx_rtp',
    'nx_utils',
    'nx_vms_api',
    'nx_vms_client_core',
    'nx_vms_common',
    'nx_vms_rules',
    'nx_vms_update',
    'nx_vms_utils',
    'nx_zip',
    'qtkeychain',
    'udt'
]

def create_mobile_client_archive(config, output_file):
    binaries_dir = Path(config['bin_source_dir'])
    current_binary_dir = Path(config['current_binary_dir'])
    qt_directory = Path(config['qt_directory'])
    mobile_client_qml_dir = Path(config['mobile_client_qml_dir'])
    mobile_client_binary_name = config['mobile_client_binary_name']
    locale_resources_directory = qt_directory / 'resources'
    webengine_locales_directory = qt_directory / 'translations/qtwebengine_locales'
    asan_library_name = config['asan_library_name']

    with zipfile.ZipFile(output_file, "w", zipfile.ZIP_DEFLATED) as zip:
        tools.zip_files_to(zip, tools.ffmpeg_files(binaries_dir), binaries_dir)
        tools.zip_files_to(zip, tools.openssl_files(binaries_dir), binaries_dir)
        tools.zip_files_to(zip, tools.openal_files(binaries_dir), binaries_dir)
        tools.zip_files_to(zip, tools.quazip_files_to(binaries_dir), binaries_dir)
        tools.zip_files_to(zip, tools.nx_files(binaries_dir, nx_libraries), binaries_dir)
        tools.zip_files_to(zip, tools.icu_files(binaries_dir), binaries_dir)
        tools.zip_files_to(zip, tools.find_all_files(locale_resources_directory), qt_directory)
        tools.zip_files_to(zip, tools.find_all_files(webengine_locales_directory), qt_directory)

        qt_bin_dir = qt_directory / 'bin'
        tools.zip_files_to(zip, tools.qt_libraries(qt_bin_dir, qt_libraries), qt_bin_dir)
        for qt_binary in qt_binaries:
            zip.write(qt_bin_dir / qt_binary, qt_binary)
        tools.zip_files_to(zip, tools.find_all_files(mobile_client_qml_dir),
            mobile_client_qml_dir, 'qml')

        qt_plugins_dir = qt_directory / 'plugins'
        tools.zip_files_to(zip, tools.qt_plugins_files(qt_plugins_dir, qt_plugins),
            qt_plugins_dir, 'plugins')

        tools.zip_all_files(zip, Path(config['ucrt_directory']) / 'bin')
        tools.zip_all_files(zip, Path(config['vcrt_directory']) / 'bin')
        tools.zip_all_files(zip, Path(config['fonts_directory']) / 'bin')

        zip.write(binaries_dir / 'client_core_external.dat', 'client_core_external.dat')
        zip.write(binaries_dir / 'bytedance_iconpark.dat', 'bytedance_iconpark.dat')

        translations_dir = binaries_dir / 'translations'
        for file in config['mobile_client_translation_files'].split(';'):
            zip.write(translations_dir / file, f'translations/{file}')

        if asan_library_name:
            zip.write(binaries_dir / asan_library_name, asan_library_name)

        zip.write(binaries_dir / mobile_client_binary_name, mobile_client_binary_name)
        zip.write(current_binary_dir / 'qt.conf', 'qt.conf')

        # Write metadata
        distribution_output_dir = Path(config['distribution_output_dir'])
        zip.write(distribution_output_dir / 'build_info.txt', 'metadata/build_info.txt')
        zip.write(distribution_output_dir / 'build_info.json', 'metadata/build_info.json')
        zip.write(distribution_output_dir / 'conan_refs.txt', 'metadata/conan_refs.txt')
        zip.write(distribution_output_dir / 'conan.lock', 'metadata/conan.lock')


def main():
    args = tools.parse_generation_scripts_arguments()
    create_mobile_client_archive(args.config, args.output_file)


if __name__ == '__main__':
    main()
