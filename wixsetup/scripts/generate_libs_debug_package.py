#!/bin/python2

import argparse
import os
import zipfile
import yaml


def create_libs_debug_file(binaries_dir, output_filename):
    pdb_filenames = [
        'nx_network.pdb',
        'nx_utils.pdb',
        'nx_vms_utils.pdb',
        'udt.pdb']
    with zipfile.ZipFile(output_filename, "w", zipfile.ZIP_DEFLATED) as zip:
        for pdb_filename in pdb_filenames:
            zip.write(os.path.join(binaries_dir, pdb_filename), pdb_filename)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--config', help="Config file", required=True)
    parser.add_argument('--output', help="Output directory", required=True)
    args = parser.parse_args()

    with open(args.config, 'r') as f:
        config = yaml.load(f)

    output_filename = os.path.join(
        args.output, config['libs_debug_distribution_name']) + '.zip'
    create_libs_debug_file(
        binaries_dir=config['bin_source_dir'],
        output_filename=output_filename)


if __name__ == '__main__':
    main()
