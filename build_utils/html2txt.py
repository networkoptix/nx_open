#!/usr/bin/env python

import argparse
import sys
import re


def html2txt(infile, outfile):
    source = infile.read()
    outfile.write(re.sub('<.*?>', '', source, flags=re.MULTILINE|re.DOTALL))
    return True


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('infile', nargs='?', type=argparse.FileType('r'), default=sys.stdin)
    parser.add_argument('outfile', nargs='?', type=argparse.FileType('w'), default=sys.stdout)
    args = parser.parse_args()
    return html2txt(args.infile, args.outfile)


if __name__ == "__main__":
    if not main():
        exit(1)
