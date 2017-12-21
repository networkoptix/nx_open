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
    'Sql',
    'Widgets',
    'OpenGL',
    'WebChannel',
    'PrintSupport',
    'MultimediaWidgets',
    'MultimediaQuick_p',
    'Qml',
    'Quick',
    'QuickWidgets',
    'LabsTemplates',
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

nx_libraries = ['nx_vms_utils', 'nx_utils', 'nx_network', 'nx_kit', 'udt']


def create_client_update_file(
    binaries_dir,
    qt_dir,
    vcredist_directory,
    vox_dir,
    fonts_dir,
    client_qml_dir,
    help_directory,
    client_binary_name,
    launcher_version_name,
    minilauncher_binary_name,
    client_update_files_directory,
    output_file
):
    with zipfile.ZipFile(output_file, "w", zipfile.ZIP_DEFLATED) as zip:
        e.zip_files_to(zip, e.ffmpeg_files(binaries_dir), binaries_dir)
        e.zip_files_to(zip, e.openssl_files(binaries_dir), binaries_dir)
        e.zip_files_to(zip, e.openal_files(binaries_dir), binaries_dir)
        e.zip_files_to(zip, e.quazip_files_to(binaries_dir), binaries_dir)
        e.zip_files_to(zip, e.nx_files(binaries_dir, nx_libraries), binaries_dir)
        e.zip_files_to(zip, e.find_all_files(client_update_files_directory),
                       client_update_files_directory)

        qt_bin_dir = os.path.join(qt_dir, 'bin')
        e.zip_files_to(zip, e.icu_files(qt_bin_dir), qt_bin_dir)
        e.zip_files_to(zip, e.qt_files(qt_bin_dir, qt_libraries), qt_bin_dir)
        e.zip_files_to(zip, e.find_all_files(client_qml_dir), client_qml_dir, 'qml')

        qt_plugins_dir = os.path.join(qt_dir, 'plugins')
        e.zip_files_to(zip, e.qt_plugins_files(qt_plugins_dir, qt_plugins), qt_plugins_dir)

        e.zip_rdep_package_to(zip, help_directory)
        e.zip_rdep_package_to(zip, vcredist_directory)
        e.zip_rdep_package_to(zip, vox_dir)
        e.zip_rdep_package_to(zip, fonts_dir)

        zip.write(os.path.join(binaries_dir, 'desktop_client.exe'), client_binary_name + '.exe')
        zip.write(os.path.join(binaries_dir, 'applauncher.exe'), 'applauncher.exe')
        zip.write(os.path.join(binaries_dir, launcher_version_name), launcher_version_name)
        zip.write(os.path.join(binaries_dir, 'minilauncher.exe'), minilauncher_binary_name)


'''
<?if $(var.quicksync) = true ?>
<File Id="quicksyncdecoder.dll" Source="$(var.PluginsDir)/quicksyncdecoder.dll" DiskId="13"/>
<File Id="hw_decoding_conf.xml" Source="$(var.PluginsDir)/hw_decoding_conf.xml" DiskId="13"/>
<?endif?>
'''


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--config', help="Config file", required=True)
    parser.add_argument('--output', help="Output directory", required=True)
    args = parser.parse_args()

    with open(args.config, 'r') as f:
        config = yaml.load(f)

    output_file = os.path.join(
        args.output, config['client_update_distribution_name']) + '.zip'
    create_client_update_file(
        binaries_dir=config['bin_source_dir'],
        qt_dir=config['qt_directory'],
        vcredist_directory=config['vcredist_directory'],
        vox_dir=config['festival_vox_directory'],
        fonts_dir=config['fonts_directory'],
        client_qml_dir=config['client_qml_dir'],
        help_directory=config['help_directory'],
        client_binary_name=config['client_binary_name'],
        launcher_version_name=config['launcher_version_file'],
        minilauncher_binary_name=config['minilauncher_binary_name'],
        client_update_files_directory=config['client_update_files_directory'],
        output_file=output_file)


if __name__ == '__main__':
    main()
