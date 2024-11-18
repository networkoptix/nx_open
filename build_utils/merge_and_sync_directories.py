#!/usr/bin/env python3

## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import sys
import os
import argparse
import shutil
import filecmp


def update_file(src_file, dst_file):
    dir = os.path.dirname(dst_file)
    if not os.path.isdir(dir):
        os.makedirs(dir)

    if not os.path.exists(dst_file):
        print(f"Copying {src_file} -> {dst_file}", file=sys.stderr)
        shutil.copyfile(src_file, dst_file)
    elif not filecmp.cmp(src_file, dst_file):
        print(f"Updating {src_file} -> {dst_file}", file=sys.stderr)
        shutil.copyfile(src_file, dst_file)


def merge_and_sync_directories(sources, output_dir):
    needed_files = {}
    needed_dirs = set()

    for rule in sources:
        dst, entries = rule.split(':', 1) if ':' in rule else (".", rule)
        dst = os.path.normpath(os.path.join(output_dir, dst))
        entries = entries.split(',')

        for entry in entries:
            if os.path.isdir(entry):
                for root, _, files in os.walk(entry):
                    for f in files:
                        src_path = os.path.join(root, f)
                        rel_path = os.path.relpath(src_path, entry)
                        dst_path = os.path.normpath(os.path.join(dst, rel_path))
                        needed_files[dst_path] = src_path
                        needed_dirs.add(os.path.dirname(dst_path))
            else:
                dst_path = os.path.join(dst, os.path.basename(entry))
                needed_files[dst_path] = entry
                needed_dirs.add(os.path.dirname(dst_path))

    unneeded_files = []
    unneeded_dirs = []
    for root, dirs, files in os.walk(output_dir):
        for f in files:
            file_path = os.path.normpath(os.path.join(root, f))
            if file_path not in needed_files:
                unneeded_files.append(file_path)
        for d in dirs:
            dir_path = os.path.normpath(os.path.join(root, d))
            if dir_path not in needed_dirs:
                unneeded_dirs.append(dir_path)

    for f in unneeded_files:
        print(f"Removing {f}", file=sys.stderr)
        os.remove(f)

    for d in reversed(unneeded_dirs): #< Reverse the list to remove more nested dirs earlier.
        print(f"Removing {d}", file=sys.stderr)
        if os.path.isdir(d):
            shutil.rmtree(d)

    for dst, src in needed_files.items():
        update_file(src, dst)


def main():
    parser = argparse.ArgumentParser(
        description="Merge or update a directory from other multiple directories and files.")
    parser.add_argument("sources", type=str, nargs="+",
        help="Source directories and files in the form of [<destination>:]<entry>[,<entry>...]")
    parser.add_argument("output", type=str, help="Output directory")
    parser.add_argument("--dry-run", action="store_true", help="Do not do any file operations")
    args = parser.parse_args()

    merge_and_sync_directories(args.sources, args.output)


if __name__ == "__main__":
    main()
