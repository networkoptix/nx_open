#!/usr/bin/env python3

## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

from pathlib import Path
import sys
import zipfile

ALLOWED_EXTENSIONS = ['.so', '.dll']
DIR_BLACKLIST = ['CMakeFiles', 'axis_camera_plugin_autogen']


def main():
    sample_build_dir = Path(sys.argv[1])
    archive_name = sys.argv[2]

    if not sample_build_dir.is_dir():
        print(f"Error: {sample_build_dir} is not a directory.")
        sys.exit(1)

    items_to_archive = []
    for integration_type_dir in Path(sample_build_dir).glob("*/*"):
        if not integration_type_dir.is_dir():
            continue
        for item in integration_type_dir.glob("*"):
            if item.is_file() and item.suffix in ALLOWED_EXTENSIONS:
                items_to_archive.append(item)
            elif item.is_dir() and item.name not in DIR_BLACKLIST:
                items_to_archive += [i for i in item.rglob("*") if i.is_file()]


    with zipfile.ZipFile(archive_name, "w") as archive:
        for file in items_to_archive:
            rel_path = file.relative_to(sample_build_dir)
            print(f"  Adding {file} to archive as {rel_path}")
            archive.write(file, rel_path)


if __name__ == "__main__":
    main()
