#!/bin/python2

import argparse
import os
import yaml

from environment import zip_files


def create_misc_debug_file(binaries_dir, output_filename):
    pdb_filenames = [
        'applauncher.pdb',
        'media_db_util.pdb',
        'minilauncher.pdb',
        'testcamera.pdb',
        'traytool.pdb']
    zip_files(pdb_filenames, binaries_dir, output_filename, mandatory=False)


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
