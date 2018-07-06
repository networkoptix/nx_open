#!/usr/bin/env python


import argparse


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("-s", "--source-string", type=str,
        help="String to replace, as a plain text")
    parser.add_argument("-r", "--replacement-string", type=str)
    parser.add_argument("files", nargs="+", help="File to process")

    args = parser.parse_args()

    source_string = bytes(args.source_string)
    replacement_string = bytes(args.replacement_string)

    for file_name in args.files:
        with open(file_name, "rb") as f:
            data = f.read()

        data = data.replace(source_string, replacement_string)

        with open(file_name, "wb") as f:
            f.write(data)


if __name__ == "__main__":
    main()
