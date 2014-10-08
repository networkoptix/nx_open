#!/bin/python3

import argparse
import re
import os
from subprocess import call

base_dpi = 90


parser = argparse.ArgumentParser()
parser.add_argument('fileName', type=str, help='Inkscape SVG file.')
parser.add_argument('-o', '--output-dir', help='Output dir where the resulting icons will be placed.')

args = parser.parse_args()

output_dir = args.output_dir
if output_dir:
    if not os.path.exists(output_dir):
        print("Directory does not exist: {0}".format(output_dir))
        exit(1)
    if not os.path.isdir(output_dir):
        print("{0} is not a directory".format(output_dir))
        exit(1)
else:
    output_dir = os.getcwd()

svg = open(args.fileName, 'r')
file_data = svg.read()
svg.close()

exportName = os.path.splitext(os.path.basename(args.fileName))[0] + '.png'

resolutions = ['ldpi', 'mdpi', 'hdpi', 'xhdpi', 'xxhdpi', 'xxxhdpi']
mandatory = ['ldpi', 'mdpi']

found_resolutions = []

for res in resolutions:
    if file_data.find('id="{0}"'.format(res)) == -1:
        if res in mandatory:
            print("Could not find bound for {0}.".format(res))
            exit(1)
    else:
        found_resolutions.append(res)

for res in resolutions:
    path = os.path.join(output_dir, '+' + res)
    if not os.path.exists(path):
        os.mkdir(path)
    name = os.path.join(path, exportName)
    print('Exporting {0} image to {1}.'.format(res, name))

    dpi = base_dpi
    identifier = res

    if res not in found_resolutions:
        if res == 'hdpi':
            identifier = 'ldpi'
            dpi = base_dpi * 2
        elif res == 'xhdpi':
            identifier = 'mdpi'
            dpi = base_dpi * 2
        elif res == 'xxhdpi':
            if 'hdpi' in found_resolutions:
                identifier = 'hdpi'
                dpi = base_dpi * 2
            else:
                identifier = 'mdpi'
                dpi = base_dpi * 3
        elif res == 'xxxhdpi':
            if 'xhdpi' in found_resolutions:
                identifier = 'xhdpi'
                dpi = base_dpi * 2
            else:
                identifier = 'mdpi'
                dpi = base_dpi * 4
    call(['inkscape', '-i', identifier, '-d', str(dpi), '-e', name, args.fileName])

print('Export finished!')