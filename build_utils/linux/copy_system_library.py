#!/usr/bin/env python

from __future__ import print_function
import os
import sys
import subprocess
import argparse
import shutil


def get_lib_dirs_from_compiler(compiler, compiler_flags=""):
    try:
        lines = subprocess.check_output(
            "{} --print-search-dirs {}".format(compiler, compiler_flags),
            universal_newlines=True,
            shell=True).split() #< Using shell=True to avoid mess with splitting compiler_flags.
    except subprocess.CalledProcessError as e:
        print("Could not get library search dirs from the compiler.", file=sys.stderr)
        print("Command failed:", e.cmd, file=sys.stderr)
        return None

    try:
        libs = lines[lines.index("libraries:") + 1]
    except (ValueError, IndexError):
        return []

    if libs[0] == "=":
        libs = libs[1:]

    return libs.split(":")


def get_lib_dirs_from_link_flags(flags):
    result = []
    flags = flags.split()

    prefixes = ["-L", "-Wl,-rpath-link,"]

    while flags:
        flag = flags.pop(0)

        if flag == "-L":
            if flags and flags[0][0] != "-":
                result.append(flags.pop(0))
        else:
            for prefix in prefixes:
                if flag.startswith(prefix):
                    result.append(flag[len(prefix):])

    return result


def find_library(lib, lib_dirs):
    for lib_dir in lib_dirs:
        file_name = os.path.join(lib_dir, lib)
        if os.path.isfile(file_name):
            return file_name
    return None


def copy_library(file_name, target_dir, list_files=False):
    required_name = os.path.basename(file_name)

    real_file = os.path.realpath(file_name)
    real_basename = os.path.basename(real_file)

    target_file_name = os.path.join(target_dir, real_basename)
    if list_files:
        print(target_file_name)
    else:
        print("Copying {} -> {}".format(real_file, target_file_name))
    if os.path.exists(target_file_name):
        os.remove(target_file_name)
    shutil.copy2(real_file, target_file_name)

    if required_name != real_basename:
        target_symlink_name = os.path.join(target_dir, required_name)
        if list_files:
            print(target_symlink_name)
        else:
            print("Symlink {} -> {}".format(real_basename, target_symlink_name))
        if os.path.exists(target_symlink_name):
            os.remove(target_symlink_name)
        os.symlink(real_basename, target_symlink_name)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("-c", "--compiler", help="Compiler executable")
    parser.add_argument("-f", "--flags", help="Compiler flags", default="")
    parser.add_argument("-o", "--dest-dir", help="Destination directory", default=os.getcwd())
    parser.add_argument("-L", "--lib-dir", dest="lib_dirs", action="append", default=[],
        help="Additional library directory")
    parser.add_argument("-lf", "--link-flags", help="Link flags")
    parser.add_argument("-l", "--list", action="store_true", help="List created files")
    parser.add_argument("libs", nargs="+", help="Libraries to copy")

    args = parser.parse_args()

    lib_dirs = []

    if args.compiler:
        compiler_dirs = get_lib_dirs_from_compiler(args.compiler, args.flags)
        if compiler_dirs is None:
            exit(1)

        lib_dirs = compiler_dirs

    if args.link_flags:
        lib_dirs += get_lib_dirs_from_link_flags(args.link_flags)

    lib_dirs += args.lib_dirs

    libs = []
    for lib in args.libs:
        file_name = find_library(lib, lib_dirs)

        if not file_name:
            print("Failed to find library %s" % lib, file=sys.stderr)
            sys.exit(1)

        libs.append(file_name)

    for lib in libs:
        copy_library(lib, args.dest_dir, list_files=args.list)


if __name__ == "__main__":
    main()
