#!/usr/bin/env python

## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import argparse
import json
import os
from pathlib import Path
import sys


def replace_conan_qt_path(file, qtdir, build_root):
    print("Substitute actual Qt path to the {})".format(file))
    if not os.path.exists(file):
        print("File {} does not exists".format(file))
        return 1

    modified = False
    binaries_dir = (Path(qtdir) / 'bin').as_posix()
    with open(file, 'r', encoding='utf-8-sig') as source:
        json_root = json.load(source)
        for json_configuration in json_root['configurations']:
            if Path(json_configuration['buildRoot']) != build_root:
                continue

            if 'environments' not in json_configuration:
                modified = True
                json_configuration['environments'] = [{'qtdir': binaries_dir}]
            else:
                json_environments = json_configuration['environments']
                for json_environment in json_environments:
                    if 'qtdir' in json_environment and json_environment['qtdir'] == binaries_dir:
                        continue
                    modified = True
                    json_environment['qtdir'] = binaries_dir

    if modified:
        print("Storing changes to {}".format(file))
        with open(file, 'w', encoding='utf-8') as target:
            json.dump(json_root, target, indent=2)
    else:
        print("All {} configurations contain valid path".format(len(json_root['configurations'])))
    return 0


def main():
    parser = argparse.ArgumentParser(
        description="Substitute qt path in the Visual Studio cmake configuration file.")
    parser.add_argument("file", type=str, help="Path to CMakeSettings.json")
    parser.add_argument("--qtdir", type=str, help="Path to qt dir")
    parser.add_argument("--build_root", type=str, help="Build root, which should be processed")
    args = parser.parse_args()
    sys.exit(replace_conan_qt_path(args.file, args.qtdir, Path(args.build_root)))


if __name__ == "__main__":
    main()
