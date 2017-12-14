#!/bin/python2

import argparse
import os
import zipfile
import yaml


def create_misc_debug_file(binaries_dir, output_filename):
    pdb_filenames = [
        'applauncher.pdb',
        'minilauncher.pdb',
        'testcamera.pdb',
        'traytool.pdb']
    with zipfile.ZipFile(output_filename, "w", zipfile.ZIP_DEFLATED) as zip:
        for pdb_filename in pdb_filenames:
            source_file = os.path.join(binaries_dir, pdb_filename)
            if os.path.isfile(source_file):
                zip.write(source_file, pdb_filename)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--config', help="Config file", required=True)
    parser.add_argument('--output', help="Output directory", required=True)
    args = parser.parse_args()

    with open(args.config, 'r') as f:
        config = yaml.load(f)

    output_filename = os.path.join(
        args.output, config['misc_debug_distribution_name']) + '.zip'
    create_misc_debug_file(
        binaries_dir=config['bin_source_dir'],
        output_filename=output_filename)


if __name__ == '__main__':
    main()
