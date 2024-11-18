#!/usr/bin/env python3

## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/
#
# Tool for removing proprietary sections from the documentation.
#

import re
import os
import filecmp
import argparse
from pathlib import Path
from time import time

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("input", help="File to process")
    parser.add_argument("output", help="Processed file")

    args = parser.parse_args()

    with open(args.input, "rb") as f: # Binary mode to disable newline magic.
        data = f.read().decode('utf-8')

    data = data.replace('\r\n', '\n')  # Convert newlines to Unix (0x0A) format.
    # "\n{1,2}" in regex is to remove empty lines before and after the block if there are any.
    data = re.sub(r'\n{1,2}\[proprietary\].+?\[/proprietary\]\n{1,2}',
        '\n\n',
        data,
        flags=re.DOTALL)

    output_file_name = args.output

    output_file = Path(output_file_name)
    if output_file.is_file():
        # Output file exists. Generate the new version and compare with the existing one.
        tmp_output_file_name = f"{output_file_name}.{time()}"
        with open(tmp_output_file_name, "wb") as f:
            f.write(data.encode('utf-8'))

        if filecmp.cmp(output_file_name, tmp_output_file_name):
            # Files are the same, remove the new one.
            os.remove(tmp_output_file_name)
        else:
            # Files differ, remove the old version and substitute with the new one.
            os.remove(output_file_name)
            os.rename(tmp_output_file_name, output_file_name)

    else:
        # File doesn't exist. Generate it.
        with open(output_file_name, "wb") as f:
            f.write(data.encode('utf-8'))


if __name__ == "__main__":
    main()
