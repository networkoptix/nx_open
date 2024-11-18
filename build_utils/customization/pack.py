#!/usr/bin/env python3

## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import argparse
import json
import logging
import sys
from typing import Dict, Generator, Tuple, Union
import yaml
import zipfile

from filecmp import cmp
from pathlib import Path
from shutil import copy2, rmtree

FILEMAP_NAME = 'filemap.yaml'

FileMap = Dict[str, Union[str, 'FileMap']]
FileMapElement = Tuple[Path, str]


def find_filemap() -> FileMap:
    config_path = Path(__file__).parent / FILEMAP_NAME
    if not config_path.exists():
        logging.error('Config file %s cannot be found', config_path)
        sys.exit(1)

    with open(config_path, 'r') as f:
        return yaml.load(f, Loader=yaml.SafeLoader)


def iterate_over_dict(dictionary: FileMap, path: Path) -> Generator[FileMapElement, None, None]:
    for key, value in dictionary.items():
        if type(value) is dict:
            yield from iterate_over_dict(value, path / key)
        else:
            yield (path / key, value)


def copy_if_different(source: Path, target: Path):
    target.parent.mkdir(exist_ok=True, parents=True)

    if target.exists() and cmp(source, target):
        logging.info('Copying %s to %s - SKIPPED', source, target)
    else:
        logging.info('Copying %s to %s', source, target)
        copy2(source, target)
    if not target.exists():
        logging.critical('File %s cannot be written', target)
        sys.exit(1)
    if not cmp(source, target):
        logging.critical('File %s integrity check failed', target)
        sys.exit(1)


def pack(customization_path: Path, output_path: Path):
    filemap = find_filemap()
    with zipfile.ZipFile(output_path, "w", zipfile.ZIP_DEFLATED) as zip:
        relative_path = Path()
        for source, target in iterate_over_dict(filemap, relative_path):
            existing_file = customization_path / target
            if not existing_file.exists():
                logging.warning('File %s not found', existing_file)
                continue
            logging.info('Packing %s to %s', existing_file, source)
            zip.write(existing_file, source)


def unpack(package_path: Path, output_path: Path, clean_package: bool):
    filemap = find_filemap()

    unpacked_package_path = output_path / '_package_'
    unpacked_package_path.mkdir(exist_ok=True, parents=True)

    with zipfile.ZipFile(package_path, "r") as zip:
        zip.extractall(unpacked_package_path)

    for source, target in iterate_over_dict(filemap, unpacked_package_path):
        if not source.exists():
            logging.warning(f'File {source} not found')
            continue
        copy_if_different(source, output_path / target)

    if clean_package:
        rmtree(unpacked_package_path)


def list_files(output_path: Path):
    filemap = find_filemap()

    unpacked_package_path = output_path / '_package_'
    for _, target in iterate_over_dict(filemap, unpacked_package_path):
        print(output_path / target)


def get_value(package_path: Path, query: str) -> str:
    with zipfile.ZipFile(package_path, "r") as archive:
        description_bytes = archive.read("description.json")

    current_object = json.loads(description_bytes.decode("utf-8"))
    try:
        for item in query.split("."):
            if isinstance(current_object, dict):
                current_object = current_object[item]
            elif isinstance(current_object, list) and item.isnumeric():
                current_object = current_object[int(item)]
            else:
                raise KeyError
    except KeyError:
        return f'Bad query {query!r}: item {item!r}" is not found in {current_object!r}.'
    except IndexError:
        return f'Bad query {query!r}: index {item!r} is out of list {current_object!r} boundary.'

    return str(current_object)


def _add_pack_command(subparsers):

    def pack_command(args):
        return pack(args.input, args.output)

    parser = subparsers.add_parser('pack', help='Pack existing customization')
    parser.add_argument('input', type=Path, help='Unpacked customization path')
    parser.add_argument('output', type=Path, help='Package path')
    parser.add_argument('-v', '--verbose', help='Verbose output', action='store_true')
    parser.add_argument('-l', '--log', help='Log file path')
    parser.set_defaults(func=pack_command)


def _add_unpack_command(subparsers):

    def unpack_command(args):
        return unpack(args.input, args.output, args.clean)

    parser = subparsers.add_parser('unpack', help='Unpack customization package')
    parser.add_argument('input', type=Path, help='Package path')
    parser.add_argument('output', type=Path, help='Unpacked customization path')
    parser.add_argument('-v', '--verbose', help='Verbose output', action='store_true')
    parser.add_argument('-l', '--log', help='Log file path')
    parser.add_argument('-c', '--clean', help='Remove unpacked package', action='store_true')
    parser.set_defaults(func=unpack_command)


def _add_list_command(subparsers):

    def list_command(args):
        return list_files(args.input)

    parser = subparsers.add_parser('list', help='List customization package')
    parser.add_argument('input', type=Path, help='Unpacked customization path')
    parser.add_argument('-v', '--verbose', help='Verbose output', action='store_true')
    parser.add_argument('-l', '--log', help='Log file path')
    parser.set_defaults(func=list_command)


def _add_get_value_command(subparsers):

    def get_value_command(args):
        print(get_value(args.input, args.query))

    parser = subparsers.add_parser(
        'get_value',
        help=(
            'Get the value determined by the parameter "query" from the customization package '
            'description.'))
    parser.add_argument('input', type=Path, help='Package path')
    parser.add_argument('query', help='Query in the form of a JSON dot-notation expression')
    parser.add_argument('-v', '--verbose', help='Verbose output', action='store_true')
    parser.add_argument('-l', '--log', help='Log file path')
    parser.set_defaults(func=get_value_command)


def main():
    parser = argparse.ArgumentParser()

    subparsers = parser.add_subparsers(title="actions", dest='cmd')
    _add_pack_command(subparsers)
    _add_unpack_command(subparsers)
    _add_list_command(subparsers)
    _add_get_value_command(subparsers)

    args = parser.parse_args()

    log_level = logging.INFO if args.verbose else logging.WARNING

    if args.log:
        Path(args.log).parent.mkdir(exist_ok=True, parents=True)
        logging.basicConfig(
            level=logging.DEBUG,
            format='%(asctime)s %(levelname)-8s %(message)s',
            filename=args.log,
            filemode='w')
    else:
        logging.basicConfig(
            format='%(levelname)-8s %(message)s',
            level=log_level)

    args.func(args)


if __name__ == "__main__":
    main()
