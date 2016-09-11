#!/bin/env python

import argparse
import re
import os

def scan_file(file_name):
    return { os.path.basename(file_name): os.path.abspath(file_name) }

def scan_dir(dir_name, ignored_ext):
    result = {}

    for root, _, files in os.walk(dir_name):
        rel_dir = os.path.relpath(root, dir_name)
        if rel_dir == ".":
            rel_dir = ""

        for file_name in files:
            _, ext = os.path.splitext(file_name)
            if ext and ext[1:] in ignored_ext:
                continue

            result[os.path.join(rel_dir, file_name)] = os.path.abspath(os.path.join(root, file_name))

    return result

def scan_sources(inputs, ignored_ext):
    files = {}

    for input in inputs:
        if os.path.isfile(input):
            files.update(scan_file(input))
        else:
            files.update(scan_dir(input, ignored_ext))

    return files

def write_qrc(files, qrc_name):
    with open(qrc_name, "w") as out_file:
        out_file.write("<!DOCTYPE RCC>\n")
        out_file.write("<RCC version=\"1.0\">\n")
        out_file.write("<qresource prefix=\"/\">\n")

        for alias, path in files.items():
            out_file.write("    <file alias=\"{0}\">{1}</file>\n".format(alias, path))

        out_file.write("</qresource>\n")
        out_file.write("</RCC>\n")

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("-o", "--output", type=str, required=True, help="Output qrc file.")
    parser.add_argument("--ignore", type=str, nargs="*", help="Output qrc file.")
    parser.add_argument("inputs", type=str, nargs="+", help="Input files and dirs.")

    args = parser.parse_args()

    files = scan_sources(args.inputs, args.ignore if args.ignore != None else [])
    write_qrc(files, args.output)

if __name__ == "__main__":
    main()
