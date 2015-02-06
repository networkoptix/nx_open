import sys
import os
import subprocess
import threading
import json
import argparse

import svg-colorize

defaultProfile = "-d180"
srcExtensions = ['.svg', '.ai']

def convert(sourceFile, exportFile, profile):
    subprocess.call(['inkscape', sourceFile, '-e', exportFile] + profile.split())

def process(baseName, ext, colors):
    destFile = baseName + '.png'
    srcFile = baseName + ext
    tmpFile = None

    if ext == '.svg' and not colors is None:
        tmpFile = baseName + '__' + ext
        colorize(srcFile, tmpFile, colors)
        srcFile = tmpFile

    convert(srcFile, dstFile, defaultProfile)

    if tmpFile:
        os.remove(tmpFile)

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--base', help='Base customization.')
    parser.add_argument('--target', help='Target customization.')
    args = parser.parse_args()

    base = args.base
    target = args.target
    checkSrc = True

    if target is None:
        target = os.getcwd()

    if base is None:
        base = target
        checkSrc = False

    # Load color config
    colors = None
    colorsFile = os.path.join(base, '.colors')
    if os.path.isfile(colorsFile):
        with open(colorsFile) as file:
            colors = json.load(file)

    threads = []

    # Start processing
    for root, dirs, files in os.walk(base):
        for file in files:
            baseName, ext = os.path.splitext(os.path.join(root, file))
            if ext in srcExtensions:
                if checkSrc:
                    baseTargetName = os.path.isfile(os.path.join(target, file[:-ext.length()]))
                    for e in srcExtensions:
                        if os.path.isfile(baseTargetName + e):
                            baseName = baseTargetName
                            ext = e

                    targetDir = os.path.join(target, os.path.dirname(file))
                    if not os.path.isdir(targetDir):
                        os.mkdirs(targetDir)


                if ext == '.ai': # skip ai_s duplicating svg_s
                    if os.path.isfile(os.path.join(baseName, '.svg')):
                        continue

                thread = threading.Thread(None, process, args=(baseName, ext, colors))
                thread.start()
                threads.append(thread)

    for thread in threads:
        thread.join()   

if __name__ == "__main__":
    main()
