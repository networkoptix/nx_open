#!/usr/bin/env python2

import argparse
import logging
import os
import sys
import yaml
import zipfile

from filecmp import cmp
from pathlib import Path
from shutil import copy2

FILEMAP_NAME = 'filemap.yaml'


def find_filemap():
    config_path = Path(__file__).parent / FILEMAP_NAME
    if not config_path.exists():
        logging.error('Config file %s cannot be found', config_path)
        sys.exit(1)

    with open(config_path.as_posix(), 'r') as f:
        return yaml.load(f)


def iterate_over_dict(dictionary, path):
    for key, value in dictionary.items():
        if type(value) is dict:
            for nested_key, nested_value in iterate_over_dict(value, path / key):
                yield (nested_key, nested_value)
        else:
            yield (path / key, value)


def copy_if_different(source, target):
    try:
        os.makedirs(target.parent.as_posix())
    except Exception:
        pass

    if target.exists() and cmp(source.as_posix(), target.as_posix()):
        logging.info('Copying %s to %s - SKIPPED', source, target)
    else:
        logging.info('Copying %s to %s', source, target)
        copy2(source.as_posix(), target.as_posix())


def pack(customization_path, output_path):
    filemap = find_filemap()
    with zipfile.ZipFile(output_path.as_posix(), "w", zipfile.ZIP_DEFLATED) as zip:
        relative_path = Path()
        for source, target in iterate_over_dict(filemap, relative_path):
            existing_file = customization_path / target
            if not existing_file.exists():
                logging.warning('File %s not found', existing_file)
                continue
            logging.info('Packing %s to %s', existing_file, source.as_posix())
            zip.write(existing_file.as_posix(), source.as_posix())


def unpack(package_path, output_path):
    filemap = find_filemap()

    unpacked_package_path = output_path / '_package_'
    try:
        os.makedirs(unpacked_package_path.as_posix())
    except Exception:
        pass

    with zipfile.ZipFile(package_path.as_posix(), "r") as zip:
        zip.extractall(unpacked_package_path.as_posix())

    for source, target in iterate_over_dict(filemap, unpacked_package_path):
        if not source.exists():
            logging.warning('File {} not found'.format(source))
            continue
        copy_if_different(source, output_path / target)


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
