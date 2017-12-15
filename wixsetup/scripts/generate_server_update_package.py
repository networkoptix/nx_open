#!/bin/python2

import argparse
import os
import yaml
import zipfile

from environment import zip_files_to, find_all_files


def generate_server_update_package(
    distribution_output_dir,
    server_distribution_name,
    server_update_files_firectory,
    output_filename
):
    with zipfile.ZipFile(output_filename, "w", zipfile.ZIP_DEFLATED) as zip:
        distribution_file = os.path.join(distribution_output_dir, server_distribution_name + '.exe')
        zip_files_to(zip, [distribution_file], distribution_output_dir)
        zip_files_to(zip, find_all_files(server_update_files_firectory),
                     server_update_files_firectory)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--config', help="Config file", required=True)
    parser.add_argument('--output', help="Output directory", required=True)
    args = parser.parse_args()

    with open(args.config, 'r') as f:
        config = yaml.load(f)

    output_filename = os.path.join(
        args.output, config['server_update_distribution_name']) + '.zip'
    generate_server_update_package(
        distribution_output_dir=config['distribution_output_dir'],
        server_distribution_name=config['server_distribution_name'],
        server_update_files_firectory=config['server_update_files_firectory'],
        output_filename=output_filename)


if __name__ == '__main__':
    main()
