#!/bin/python3

import argparse
import os

parser = argparse.ArgumentParser()
parser.add_argument('modules', metavar='MODULES', type=str, nargs='+', help="Modules to include.")
parser.add_argument('-p', '--path-suffix', type=str, help="Relative path to module .pro file (now it's processor arch name).")
parser.add_argument('-o', '--output', type=str, help="Output .pro file name.")

args = parser.parse_args()

pro_file = open(args.output, 'w')

pro_file.write('TEMPLATE = subdirs\n\n')
pro_file.write('CONFIG = ordered\n\n')
pro_file.write('SUBDIRS = \\\n')

for module in args.modules:
    pro_file.write('    {}.pro \\\n'.format(os.path.join(module, args.path_suffix, module)))

pro_file.write('\n')

pro_file.close()
