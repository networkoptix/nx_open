#!/bin/python3

## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

from distribution_tools import zip_files_to, parse_generation_scripts_arguments
from pathlib import Path
import zipfile


def create_client_debug_file(build_dir, output_filename):
    dsym_paths = [
        build_dir / 'mobile_client.dSYM',
        build_dir / 'push_notification_extension.dSYM',
    ]

    # Archive each .dSYM directory.
    with zipfile.ZipFile(output_filename, "w", zipfile.ZIP_DEFLATED) as zip:
        for dsym_dir in dsym_paths:
            zip_files_to(zip, dsym_dir.rglob('*'), dsym_dir.parent)


def main():
    args = parse_generation_scripts_arguments()

    create_client_debug_file(
        build_dir=Path(args.config['BUILD_DIR']),
        output_filename=args.output_file)


if __name__ == '__main__':
    main()
