#!/usr/bin/env python3

## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import sys
import argparse

parser = argparse.ArgumentParser()
parser.add_argument('--input', type=argparse.FileType('r'), default=sys.stdin, help="Source transaction description file")
parser.add_argument('--output', type=argparse.FileType('w'), default=sys.stdout, help="Output file with unique type list")

args = parser.parse_args()

dataTypes = set()
for line in args.input:
    line = line.strip()
    if line.startswith('APPLY'):
        dataTypes.add(line.split(',')[2].strip())

args.output.write('#define TransactionDataTypes')
for dataType in dataTypes:
    args.output.write(' \\\n({})'.format(dataType))
args.output.write('\n')
