#!/bin/python2

import argparse
import os
import zipfile
import yaml

import environment as e

qt_libraries = [
    'Core',
    'Gui',
    'Multimedia',
    'Network',
    'Xml',
    'XmlPatterns',
    'Concurrent',
    'Svg',
    'Sql',
    'Widgets',
    'OpenGL',
    'WebChannel',
    'PrintSupport',
    'MultimediaWidgets',
    'MultimediaQuick',
    'Qml',
    'Quick',
    'QuickWidgets',
    'QuickTemplates2',
    'QuickControls2',
    'WebKit',
    'WebKitWidgets']

qt_plugins = [
    'imageformats/qgif.dll',
    'imageformats/qjpeg.dll',
    'imageformats/qtiff.dll',
    'mediaservice/dsengine.dll',
    'mediaservice/qtmedia_audioengine.dll',
    'audio/qtaudio_windows.dll',
    'platforms/qwindows.dll']

nx_libraries = [
    'nx_vms_client_desktop',
    'nx_vms_utils',
    'nx_vms_applauncher_api',
    'nx_utils',
    'nx_sql',
    'nx_network',
    'nx_kit',
    'nx_vms_api',
    'vms_gateway_core',
    'udt',
    'qtkeychain'
]


def create_client_update_file(config, output_file):
    icu_directory = config['icu_directory']
    binaries_dir = config['bin_source_dir']
    current_binary_dir = config['current_binary_dir']
    qt_directory = config['qt_directory']
    client_qml_dir = config['client_qml_dir']
    client_binary_name = config['client_binary_name']
    launcher_version_name = config['launcher_version_file']
    minilauncher_binary_name = config['minilauncher_binary_name']
    client_update_files_directory = config['client_update_files_directory']
    fonts_directory = config['fonts_directory']

    with zipfile.ZipFile(output_file, "w", zipfile.ZIP_DEFLATED) as zip:
        e.zip_files_to(zip, e.ffmpeg_files(binaries_dir), binaries_dir)
        e.zip_files_to(zip, e.openssl_files(binaries_dir), binaries_dir)
        e.zip_files_to(zip, e.openal_files(binaries_dir), binaries_dir)
        e.zip_files_to(zip, e.quazip_files_to(binaries_dir), binaries_dir)
        e.zip_files_to(zip, e.nx_files(binaries_dir, nx_libraries), binaries_dir)
        e.zip_files_to(zip, e.find_all_files(client_update_files_directory),
                       client_update_files_directory)
        e.zip_files_to(zip, e.icu_files(icu_directory), icu_directory)
        e.zip_files_to(zip, e.find_all_files(fonts_directory), binaries_dir)

        qt_bin_dir = os.path.join(qt_directory, 'bin')
        e.zip_files_to(zip, e.qt_files(qt_bin_dir, qt_libraries), qt_bin_dir)
        e.zip_files_to(zip, e.find_all_files(client_qml_dir), client_qml_dir, 'qml')

        qt_plugins_dir = os.path.join(qt_directory, 'plugins')
        e.zip_files_to(zip, e.qt_plugins_files(qt_plugins_dir, qt_plugins), qt_plugins_dir)

        e.zip_rdep_package_to(zip, config['help_directory'])
        e.zip_rdep_package_to(zip, config['ucrt_directory'])
        e.zip_rdep_package_to(zip, config['vcrt_directory'])
        e.zip_rdep_package_to(zip, config['festival_vox_directory'])

        zip.write(os.path.join(binaries_dir, client_binary_name), client_binary_name)
        zip.write(os.path.join(binaries_dir, 'applauncher.exe'), 'applauncher.exe')
        zip.write(os.path.join(binaries_dir, launcher_version_name), launcher_version_name)
        zip.write(os.path.join(binaries_dir, minilauncher_binary_name), minilauncher_binary_name)
        zip.write(os.path.join(current_binary_dir, 'qt.conf'), 'qt.conf')
        zip.write(os.path.join(config['cmake_binary_dir'], 'build_info.txt'), 'build_info.txt')


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--config', help="Config file", required=True)
    parser.add_argument('--output', help="Output directory", required=True)
    args = parser.parse_args()

    with open(args.config, 'r') as f:
        config = yaml.load(f)

    output_file = os.path.join(
        args.output, config['client_update_distribution_name']) + '.zip'
    create_client_update_file(config, output_file)


if __name__ == '__main__':
    main()
