#!/bin/python2

import argparse
import os
import yaml

from environment import zip_files


def create_cloud_debug_file(binaries_dir, output_filename):
    pdb_filenames = [
        'cloud_connect_test_util.pdb',
        'cloud_db.pdb',
        'connection_mediator.pdb',
        'vms_gateway.pdb']
    zip_files(pdb_filenames, binaries_dir, output_filename)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--config', help="Config file", required=True)
    parser.add_argument('--output', help="Output directory", required=True)
    args = parser.parse_args()

    with open(args.config, 'r') as f:
        config = yaml.load(f, Loader=yaml.SafeLoader)

    output_filename = os.path.join(
        args.output, config['cloud_debug_distribution_name']) + '.zip'
    create_cloud_debug_file(
        binaries_dir=config['bin_source_dir'],
        output_filename=output_filename)


if __name__ == '__main__':
    main()
