import json
import re
import argparse

def colorize(sourceFile, destFile, colors):
    svg = ""
    with open(sourceFile, 'r') as file:
        svg = file.read()

    regEx = re.compile("#[0-9a-fA-F]{6}")

    if type(colors) is dict:
        replacer = lambda m: colors.get(m.group(0).lower(), m.group(0))
    else:
        replacer = colors

    svg = regEx.sub(replacer, svg)
    
    if destFile:
        with open(destFile, 'w') as file:
            file.write(svg)
    else:
        print svg

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('fileName', type=str, help='Source SVG file.')
    parser.add_argument('-o', '--output-file', help='Output file.')
    parser.add_argument('-c', '--colors', help='Configuration JSON.')
    parser.add_argument('--src-color', help='Source color to replace.')
    parser.add_argument('--dst-color', help='Target color to replace.')
    args = parser.parse_args()
    
    colors = {}
    if args.dst_color:
        if args.src_color:
            colors[args.src_color] = args.dst_color
        else:
            colors = args.dst_color
    else:
        with open(args.colors, 'r') as file:
            colors = json.load(file)

    colorize(args.fileName, args.output_file, colors)

if __name__ == "__main__":
    main()
