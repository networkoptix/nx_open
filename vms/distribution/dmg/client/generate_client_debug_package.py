#!/bin/python3

## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

from distribution_tools import zip_files_to, parse_generation_scripts_arguments
from pathlib import Path
import zipfile


def create_client_debug_file(app_name, build_dir, output_filename):
    bin_dir = Path(build_dir) / 'bin'
    lib_dir = Path(build_dir) / 'lib'

    dsym_paths = [
        bin_dir / f'{app_name}.dSYM',
        lib_dir / 'libnx_vms_client_desktop.dylib.dSYM'
    ]

    # Archive each .dSYM directory.
    with zipfile.ZipFile(output_filename, "w", zipfile.ZIP_DEFLATED) as zip:
        for dsym_dir in dsym_paths:
            zip_files_to(zip, dsym_dir.rglob('*'), dsym_dir.parent)


def main():
    args = parse_generation_scripts_arguments()

    create_client_debug_file(
        app_name=args.config['DISPLAY_PRODUCT_NAME'],
        build_dir=args.config['BUILD_DIR'],
        output_filename=args.output_file)


if __name__ == '__main__':
    main()
