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
if tools.is_linux():
    qt_libraries += [
        'DBus',
        'XcbQpa',
    ]

qt_binaries = [
    tools.executable_filename('QtWebEngineProcess'),
]

qt_plugins = [
    'imageformats',
    'multimedia',
    'platforminputcontexts',
    'platforms',
    'tls',
    'webview',
]
if tools.is_linux():
    qt_plugins += [
        'xcbglintegrations',
    ]

nx_libraries = [
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
    'nx_telemetry',
    'nx_utils',
    'nx_vms_api',
    'nx_vms_client_core',
    'nx_vms_common',
    'nx_vms_rules',
    'nx_vms_update',
    'nx_vms_utils',
    'nx_zip',
    'udt',
]
if tools.is_linux():
    nx_libraries += [
        'appserver2',
        'cloud_db_client',
        'nx_audio',
    ]

if tools.is_linux():
    sys_libs = [
        'libavcodec.so.61',
        'libavfilter.so.10',
        'libavformat.so.61',
        'libavutil.so.59',
        'libcrypto.so.1.1',
        'libcudart.so.12',
        'libmfx-gen.so.1.2',
        'libmfx.so.1',
        'libmfxhw64.so.1',
        'libqtkeychain.so.0.9.0',
        'libssl.so.1.1',
        'libswresample.so.5',
        'libswscale.so.8',
        'libva-drm.so.2',
        'libva-x11.so.2',
        'libva.so.2',
        'libvpl.so.2',
    ]
else:
    sys_libs = [
        'cudart64_12.dll',
        'libvpl.dll',
        'qtkeychain.dll',
    ]


def create_mobile_client_archive(config, output_file):
    binaries_dir = Path(config['bin_source_dir'])
    current_binary_dir = Path(config['current_binary_dir'])
    qt_directory = Path(config['qt_directory'])
    mobile_client_qml_dir = Path(config['mobile_client_qml_dir'])
    lib_dir = Path(config['cmake_binary_dir']) / 'lib'
    mobile_client_binary_name = config['mobile_client_binary_name']
    crashpad_handler_path = config['crashpad_handler_path']
    asan_library_name = config['asan_library_name']
    font_dir = config['fonts_directory']

    target_bin_dir = 'bin' if tools.is_linux() else '.'
    target_lib_dir = 'lib' if tools.is_linux() else '.'
    target_libexec_dir = 'libexec' if tools.is_linux() else '.'
    target_translation_dir = 'translations'

    translations_dir = binaries_dir / target_translation_dir
    locale_resources_directory = qt_directory / 'resources'
    webengine_locales_directory = qt_directory / 'translations/qtwebengine_locales'
    qt_lib_dir = qt_directory / 'lib' if tools.is_linux() else qt_directory / 'bin'
    sys_lib_dir = lib_dir if tools.is_linux() else binaries_dir
    qt_libexec_dir = qt_directory / 'libexec'
    qt_plugins_dir = qt_directory / 'plugins'
    qt_bin_dir = qt_libexec_dir if tools.is_linux() else qt_lib_dir

    with zipfile.ZipFile(output_file, "w", zipfile.ZIP_DEFLATED) as zip:
        tools.zip_files_to(zip, tools.ffmpeg_files(binaries_dir), binaries_dir)
        tools.zip_files_to(zip, tools.openssl_files(binaries_dir), binaries_dir)
        tools.zip_files_to(zip, tools.openal_files(sys_lib_dir), sys_lib_dir, target_lib_dir)
        tools.zip_files_to(zip, tools.quazip_files_to(sys_lib_dir), sys_lib_dir, target_lib_dir)
        tools.zip_files_to(zip, tools.nx_files(sys_lib_dir, nx_libraries), sys_lib_dir, target_lib_dir)
        tools.zip_files_to(zip, tools.icu_files(sys_lib_dir), sys_lib_dir, target_lib_dir)
        tools.zip_files_to(zip, tools.find_all_files(locale_resources_directory), qt_directory)
        tools.zip_files_to(zip, tools.find_all_files(webengine_locales_directory), qt_directory)
        tools.zip_files_to(
            zip, tools.qt_libraries(qt_lib_dir, qt_libraries), qt_lib_dir, target_lib_dir)

        for lib in sys_libs:
            zip.write(sys_lib_dir / lib, f'{target_lib_dir}/{lib}')

        for qt_binary in qt_binaries:
            zip.write(qt_bin_dir / qt_binary, f'{target_libexec_dir}/{qt_binary}')

        tools.zip_files_to(zip, tools.find_all_files(mobile_client_qml_dir),
            mobile_client_qml_dir, 'qml')

        for qt_plugin_dir in qt_plugins:
            tools.zip_all_files(zip, qt_plugins_dir / qt_plugin_dir, 'plugins/' + qt_plugin_dir)

        if tools.is_windows():
            tools.zip_all_files(zip, Path(config['ucrt_directory']) / 'bin')
            tools.zip_all_files(zip, Path(config['vcrt_directory']) / 'bin')

        tools.zip_files_to(zip, tools.find_all_files(font_dir), binaries_dir, target_bin_dir)

        zip.write(
            binaries_dir / 'client_core_external.dat',
            f'{target_bin_dir}/client_core_external.dat')
        zip.write(
            binaries_dir / 'bytedance_iconpark.dat',
            f'{target_bin_dir}/bytedance_iconpark.dat')
        zip.write(
            binaries_dir / 'mobile_client_external.dat',
            f'{target_bin_dir}/mobile_client_external.dat')

        for file in config['mobile_client_translation_files'].split(';'):
            zip.write(translations_dir / file, f'{target_bin_dir}/{target_translation_dir}/{file}')

        if asan_library_name:
            zip.write(binaries_dir / asan_library_name, asan_library_name)

        zip.write(
            binaries_dir / mobile_client_binary_name,
            f'{target_bin_dir}/{mobile_client_binary_name}')
        zip.write(
            crashpad_handler_path,
            f'{target_bin_dir}/{Path(crashpad_handler_path).name}')
        zip.write(current_binary_dir / 'qt.conf', target_bin_dir + '/qt.conf')
        if tools.is_linux():
            zip.write(current_binary_dir / 'qt.conf', target_libexec_dir + '/qt.conf')

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
