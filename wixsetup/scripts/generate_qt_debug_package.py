#!/bin/python2

import argparse
import os
import yaml
import zipfile

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
    'imageformats/qgif.pdb',
    'imageformats/qjpeg.pdb',
    'imageformats/qtiff.pdb',
    'mediaservice/dsengine.pdb',
    'mediaservice/qtmedia_audioengine.pdb',
    'audio/qtaudio_windows.pdb',
    'platforms/qwindows.pdb']


def generate_qt_debug_package(qt_directory, output_filename):
    qt_bin_dir = os.path.join(qt_directory, 'bin')
    qt_plugins_dir = os.path.join(qt_directory, 'plugins')
    with zipfile.ZipFile(output_filename, "w", zipfile.ZIP_DEFLATED) as zip:
        e.zip_files_to(zip, e.qt_files(qt_bin_dir, qt_libraries, extension='pdb'), qt_bin_dir)
#        e.zip_files_to(zip, e.find_all_files(client_qml_dir), client_qml_dir, 'qml')
        e.zip_files_to(zip, e.qt_plugins_files(qt_plugins_dir, qt_plugins), qt_plugins_dir)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--config', help="Config file", required=True)
    parser.add_argument('--output', help="Output directory", required=True)
    args = parser.parse_args()

    with open(args.config, 'r') as f:
        config = yaml.load(f)

    output_filename = os.path.join(args.output, 'qt_pdb.zip')
    generate_qt_debug_package(
        qt_directory=config['qt_directory'],
        output_filename=output_filename)


if __name__ == '__main__':
    main()
