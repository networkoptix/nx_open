#!/bin/python2

import argparse
import fnmatch
import os
import zipfile
import yaml

common_qt_libraries = [
    'Core',
    'Gui']

client_qt_libraries = [
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

client_qt_plugins = [
    'imageformats/qgif.dll',
    'imageformats/qjpeg.dll',
    'imageformats/qtiff.dll',
    'mediaservice/dsengine.dll',
    'mediaservice/qtmedia_audioengine.dll',
    'audio/qtaudio_windows.dll',
    'platforms/qwindows.dll']

common_nx_libraries = ['nx_utils', 'nx_network', 'nx_kit', 'udt']
client_nx_libraries = ['nx_vms_utils']


def find_files_by_template(dir, template):
    for file in fnmatch.filter(os.listdir(dir), template):
        yield os.path.join(dir, file)


def find_all_files(dir):
    for root, dirs, files in os.walk(dir):
        for file in files:
            yield os.path.join(root, file)


def ffmpeg_files(source_dir):
    templates = [
        'av*.dll',
        'libogg*.dll',
        'libvorbis*.dll',
        'libmp3*.dll',
        'swscale-*.dll',
        'swresample-*.dll']
    for template in templates:
        for file in find_files_by_template(source_dir, template):
            yield file


def openssl_files(source_dir):
    yield os.path.join(source_dir, 'libeay32.dll')
    yield os.path.join(source_dir, 'ssleay32.dll')


def openal_files(source_dir):
    yield os.path.join(source_dir, 'OpenAL32.dll')


def quazip_files(source_dir):
    yield os.path.join(source_dir, 'quazip.dll')


'''
<?if $(var.quicksync) = true ?>
<File Id="quicksyncdecoder.dll" Source="$(var.PluginsDir)/quicksyncdecoder.dll" DiskId="13"/>
<File Id="hw_decoding_conf.xml" Source="$(var.PluginsDir)/hw_decoding_conf.xml" DiskId="13"/>
<?endif?>
'''


def icu_files(qt_bin_dir):
    for file in find_files_by_template(qt_bin_dir, 'icu*.dll'):
        yield file


def qt_files(qt_bin_dir, libs):
    for lib in libs:
        yield os.path.join(qt_bin_dir, 'Qt5{}.dll'.format(lib))


def qt_plugins_files(qt_plugins_dir):
    for lib in client_qt_plugins:
        yield os.path.join(qt_plugins_dir, lib)


def nx_files(source_dir, libs):
    for lib in libs:
        yield os.path.join(source_dir, '{}.dll'.format(lib))


def zip_files(zip, files, rel_path, target_path='.'):
    for file in files:
        target_filename = os.path.join(target_path, os.path.relpath(file, rel_path))
        zip.write(file, target_filename)


def zip_package(zip, package_directory):
    bin_directory = os.path.join(package_directory, 'bin')
    zip_files(zip, find_all_files(bin_directory), bin_directory)


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
    client_update_files_firectory,
    client_update_file
):
    with zipfile.ZipFile(client_update_file, "w", zipfile.ZIP_DEFLATED) as zip:
        zip_files(zip, ffmpeg_files(binaries_dir), binaries_dir)
        zip_files(zip, openssl_files(binaries_dir), binaries_dir)
        zip_files(zip, openal_files(binaries_dir), binaries_dir)
        zip_files(zip, quazip_files(binaries_dir), binaries_dir)
        zip_files(zip, nx_files(binaries_dir, common_nx_libraries), binaries_dir)
        zip_files(zip, nx_files(binaries_dir, client_nx_libraries), binaries_dir)
        zip_files(zip, find_all_files(client_update_files_firectory), client_update_files_firectory)

        qt_bin_dir = os.path.join(qt_dir, 'bin')
        zip_files(zip, icu_files(qt_bin_dir), qt_bin_dir)
        zip_files(zip, qt_files(qt_bin_dir, common_qt_libraries), qt_bin_dir)
        zip_files(zip, qt_files(qt_bin_dir, client_qt_libraries), qt_bin_dir)
        zip_files(zip, find_all_files(client_qml_dir), client_qml_dir, 'qml')

        qt_plugins_dir = os.path.join(qt_dir, 'plugins')
        zip_files(zip, qt_plugins_files(qt_plugins_dir), qt_plugins_dir)

        zip_package(zip, help_directory)
        zip_package(zip, vcredist_directory)
        zip_package(zip, vox_dir)
        zip_package(zip, fonts_dir)

        zip.write(os.path.join(binaries_dir, 'desktop_client.exe'), client_binary_name + '.exe')
        zip.write(os.path.join(binaries_dir, 'applauncher.exe'), 'applauncher.exe')
        zip.write(os.path.join(binaries_dir, launcher_version_name), launcher_version_name)
        zip.write(os.path.join(binaries_dir, 'minilauncher.exe'), minilauncher_binary_name)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--config', help="Config file", required=True)
    parser.add_argument('--output', help="Output directory", required=True)
    args = parser.parse_args()

    with open(args.config, 'r') as f:
        config = yaml.load(f)

    client_update_file = os.path.join(
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
        client_update_files_firectory=config['client_update_files_firectory'],
        client_update_file=client_update_file)


if __name__ == '__main__':
    main()
