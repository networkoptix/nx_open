#!/usr/bin/env python3

## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

'''
Script merges translated strings and corresponding metrics file into single theme wxl file.
'''

import argparse
from typing import List
from pathlib import Path
import xml.etree.ElementTree as ElementTree

ElementTree.register_namespace('', "http://schemas.microsoft.com/wix/2006/localization")

class WxlFile():
    def __init__(self, path):
        self.tree = ElementTree.parse(path)
        self.root = self.tree.getroot()

    def save_to(self, path: Path):
        self.tree.write(path, encoding="utf-8", xml_declaration=True)


def merge_files(source_files: List[WxlFile], output_file: Path):
    first_file = source_files[0]

    for file in source_files[1:]:
        for child in file.root:
            first_file.root.append(child)

    first_file.save_to(output_file)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument(
        'files',
        nargs='+',
        type=Path,
        help="Files to merge")
    parser.add_argument(
        '-o', '--output_file',
        type=Path,
        help="Output file",
        required=True)
    args = parser.parse_args()
    merge_files(
        [WxlFile(path) for path in args.files],
        args.output_file)


if __name__ == '__main__':
    main()
