import json
import re
import argparse

def colorize(sourceFile, destFile, colors):
    svg = ""
    with open(sourceFile, 'r') as file:
        svg = file.read()

    repl = dict((re.escape(src.lower()), dst) for src, dst in colors.iteritems())
    regEx = re.compile("|".join(repl.keys()), re.IGNORECASE)
    svg = regEx.sub(lambda m: repl[re.escape(m.group(0).lower())], svg)
    
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
    parser.add_argument('--color1', help='Source color to replace.')
    parser.add_argument('--color2', help='Target color to replace.')
    args = parser.parse_args()
    
    colors = {}
    if args.color1 and args.color2:
        colors[args.color1] = args.color2
    else:
        with open(args.colors, 'r') as file:
            colors = json.load(file)

    colorize(args.fileName, args.output_file, colors)

if __name__ == "__main__":
    main()
