#!/usr/bin/env python


import sys
import os
import re
import argparse


def get_files_from_build_ninja(build_dir):
    regexes = [
        re.compile(r"^build\s+([^:]+):([^|]*)\|?([^|]*)\|?\|?(.*)$"),
        re.compile(r"^\s+COMMAND\s*=\s*.*moc(?:.exe)? @([^\s]*)$"),
        re.compile(r"^\s+depfile\s*=\s*(.+)$")
    ]

    result = set()

    try:
        with open(os.path.join(build_dir, "build.ninja")) as build_ninja:
            for line in build_ninja:
                for regex in regexes:
                    match = regex.match(line)
                    if match is None:
                        continue

                    for group in match.groups():
                        for item in group.split():
                            if os.path.isabs(item):
                                item = os.path.relpath(os.path.realpath(item), build_dir)
                                if item.startswith('..'):
                                    continue

                            result.add(item)
                    break
    except (OSError, IOError):
        sys.exit("Cannot open {}".format(os.path.join(build_dir, "build.ninja")))

    return result


def get_files_from_list_file(build_dir, list_file_name):
    result = set()

    try:
        with open(list_file_name) as list_file:
            for line in list_file:
                file_name = line.strip()

                if os.path.isabs(file_name):
                    file_name = os.path.relpath(file_name, build_dir)
                    if file_name.startswith(".."):
                        continue

                result.add(os.path.normpath(file_name))

            return result
    except (OSError, IOError):
        sys.exit("Cannot open {}".format(list_file_name))

    return result


def find_extra_files(build_dir, known_files):
    exclusions = set([
        "CTestTestfile.cmake",
        "cmake_install.cmake",
        "CMakeCache.txt",
        "build.ninja",
        "rules.ninja",
        ".ninja_deps",
        ".ninja_log"
    ])

    result = []

    for root, _, files in os.walk(build_dir):
        if "CMakeFiles" in root or "_autogen/" in root:
            continue

        relative_dir = os.path.relpath(root, build_dir)

        for file in files:
            if file in exclusions:
                continue

            file_path = os.path.normpath(os.path.join(relative_dir, file))

            if not file_path in known_files:
                result.append(os.path.join(root, file))

    return result


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("-d", "--build-dir", type=str, help="Ninja build directory.",
        default=os.getcwd())
    parser.add_argument("--list", action="store_true", help="List unknown files.")
    args = parser.parse_args()

    build_dir = os.path.abspath(args.build_dir)

    build_files = get_files_from_build_ninja(build_dir)
    known_files = get_files_from_list_file(build_dir, os.path.join(build_dir, "known_files.txt"))

    all_known_files = build_files | known_files

    extra_files = find_extra_files(build_dir, all_known_files)

    for file in extra_files:
        if args.list:
            print(file)
        else:
            print("Deleting {}".format(file))
            os.remove(file)


if __name__ == "__main__":
    main()
