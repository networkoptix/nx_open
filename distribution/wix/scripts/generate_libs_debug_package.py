#!/bin/python2

import argparse
import os
import yaml

from environment import zip_files


def create_libs_debug_file(binaries_dir, output_filename):
    pdb_filenames = [
        'nx_network.pdb',
        'nx_utils.pdb',
        'nx_sql.pdb',
        'nx_kit.pdb',
        'nx_update.pdb',
        'nx_vms_api.pdb',
        'nx_vms_utils.pdb',
        'nx_relaying.pdb',
        'vms_gateway_core.pdb',
        'udt.pdb',
        'qtkeychain.pdb'
    ]
    zip_files(pdb_filenames, binaries_dir, output_filename)


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
