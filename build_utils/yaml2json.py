#!/usr/bin/env python3

## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import argparse
import json
import yaml

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("input", type=str, help="Input YAML file name")
    parser.add_argument("output", type=str, help="Output JSON file name")

    args = parser.parse_args()
    with open(args.input, 'r') as input:
        with open(args.output, 'w') as output:
            json.dump(yaml.safe_load(input), output, indent=4)

if __name__ == '__main__':
    main()
