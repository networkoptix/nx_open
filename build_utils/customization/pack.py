#!/usr/bin/env python

import argparse
import logging
import sys
import yaml

from os import makedirs
from pathlib import Path
from shutil import copy2

FILEMAP_NAME = 'filemap.yaml'


def find_filemap():
    config_path = Path(__file__).parent / FILEMAP_NAME
    if not config_path.exists():
        logging.error('Config file %s cannot be found', config_path)
        sys.exit(1)

    with open(config_path, 'r') as f:
        return yaml.load(f, Loader=yaml.SafeLoader)


def iterate_over_dict(dictionary, path):
    for key, value in dictionary.items():
        if type(value) is dict:
            yield from iterate_over_dict(value, path / key)
        else:
            yield (path / key, value)


def copy_to(source, target):
    logging.info('Copying %s to %s', source, target)
    makedirs(target.parent, exist_ok=True)
    copy2(source, target)


def pack(customization_path, output_path):
    filemap = find_filemap()
    for source, target in iterate_over_dict(filemap, output_path):
        existing_file = customization_path / target
        if not existing_file.exists():
            logging.warning('File %s not found', existing_file)
            continue
        copy_to(existing_file, Path(source))


def unpack(package_path, output_path):
    filemap = find_filemap()
    for source, target in iterate_over_dict(filemap, package_path):
        if not source.exists():
            logging.warning('File {} not found'.format(source))
            continue
        copy_to(source, output_path / target)


def _add_pack_command(subparsers):

    def pack_command(args):
        return pack(args.input, args.output)

    parser = subparsers.add_parser('pack', help='Pack existing customization')
    parser.add_argument('input', type=Path, help='Unpacked customization path')
    parser.add_argument('output', type=Path, help='Package path')
    parser.add_argument('-v', '--verbose', help='Verbose output', action='store_true')
    parser.set_defaults(func=pack_command)


def _add_unpack_command(subparsers):

    def unpack_command(args):
        return unpack(args.input, args.output)

    parser = subparsers.add_parser('unpack', help='Unpack customization package')
    parser.add_argument('input', type=Path, help='Package path')
    parser.add_argument('output', type=Path, help='Unpacked customization path')
    parser.add_argument('-v', '--verbose', help='Verbose output', action='store_true')
    parser.set_defaults(func=unpack_command)


def main():
    parser = argparse.ArgumentParser()

    subparsers = parser.add_subparsers(title="actions", dest='cmd')
    _add_pack_command(subparsers)
    _add_unpack_command(subparsers)

    args = parser.parse_args()

    log_level = logging.INFO if args.verbose else logging.WARNING
    logging.basicConfig(
        format='%(message)s',
        level=log_level)

    args.func(args)


if __name__ == "__main__":
    main()
