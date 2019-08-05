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
    'Concurrent',
    'Sql']

nx_libraries = [
    'nx_utils',
    'nx_sql',
    'nx_network',
    'nx_kit',
    'nx_vms_api',
    'nx_vms_utils',
    'udt']


def generate_testcamera_package(config, output_file):
    icu_directory = config['icu_directory']
    binaries_dir = config['bin_source_dir']
    qt_directory = config['qt_directory']

    with zipfile.ZipFile(output_file, "w", zipfile.ZIP_DEFLATED) as zip:
        e.zip_files_to(zip, e.ffmpeg_files(binaries_dir), binaries_dir)
        e.zip_files_to(zip, e.openssl_files(binaries_dir), binaries_dir)
        e.zip_files_to(zip, e.nx_files(binaries_dir, nx_libraries), binaries_dir)
        e.zip_files_to(zip, e.icu_files(icu_directory), icu_directory)

        qt_bin_dir = os.path.join(qt_directory, 'bin')
        e.zip_files_to(zip, e.qt_files(qt_bin_dir, qt_libraries), qt_bin_dir)

        e.zip_rdep_package_to(zip, config['ucrt_directory'])
        e.zip_rdep_package_to(zip, config['vcrt_directory'])

        zip.write(os.path.join(binaries_dir, 'testcamera.exe'), 'testcamera.exe')


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--config', help="Config file", required=True)
    parser.add_argument('--output', help="Output directory", required=True)
    args = parser.parse_args()

    with open(args.config, 'r') as f:
        config = yaml.load(f)

    output_file = os.path.join(
        args.output, config['testcamera_distribution_name']) + '.zip'
    generate_testcamera_package(config, output_file)


if __name__ == '__main__':
    main()
