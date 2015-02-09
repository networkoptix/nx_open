#!/bin/env python2

import sys
import os
import subprocess
import threading
import json
import argparse
import imp

colorizer = None

defaultProfile = "-d180"
srcExtensions = ['.svg', '.ai']
defaultCustomization = 'default'
baseCustomizations = ['default', 'digitalwatchdog', 'vista']

def convert(sourceFile, exportFile, profile):
    with open(os.devnull, 'w') as devnull:
        subprocess.call(['inkscape', sourceFile, '-e', exportFile] + profile.split(), stdout = devnull)

def convertIcon(srcFile, dstFile, colors):
    tmpFile = None
    
    if not colors is None:
        path, ext = os.path.splitext(srcFile)
        tmpFile = path + '__' + ext
        colorizer.colorize(srcFile, tmpFile, colors)
        srcFile = tmpFile
        
    convert(srcFile, dstFile, defaultProfile)
    
    if tmpFile:
        os.remove(tmpFile)

def getModifiedIcons():
    p = subprocess.Popen(['hg', 'status', '-am', 'customization'], stdout=subprocess.PIPE)
    result = []
    for fileName in p.stdout:
        fileName = fileName[2:].strip()
        if fileName.endswith('.svg'):
            result.append(fileName)
    return result

def filterModified(modifiedList, customization):
    return [fileName for fileName in modifiedList if fileName.startswith(os.path.join('customization', customization))]

def getCustomizations():
    return [dirName
                for dirName in os.listdir('customization')
                    if os.path.isdir(os.path.join('customization', dirName)) and not dirName.startswith('_')
           ]

def getColors(customization):
    colors = None
    colorsFile = os.path.join('customization', customization, 'client', 'resources', '.colors')
    if os.path.isfile(colorsFile):
        with open(colorsFile) as file:
            colors = json.load(file)
    return colors

def buildCustomization(customization, baseCustomization, modified):
    colors = getColors(customization)
    
    threads = []
    root = os.path.join('customization', baseCustomization)
    
    for srcFile in modified:
        path, ext = os.path.splitext(os.path.relpath(srcFile, root))
        print '\tConverting [%s]' % (path + ext)

        #convertIcon(srcFile, os.path.join('customization', customization, path + '.png'), colors)
        thread = threading.Thread(None, convertIcon, args=(srcFile, os.path.join('customization', customization, path + '.png'), colors))
        thread.start()
        threads.append(thread)
    
    for thread in threads:
        thread.join()  
        
def main():
    dir = os.getcwd()
    if dir.endswith('util'):
        os.chdir('..')
    
    if not os.path.isdir('.hg'):
        print 'ERROR: You are not in the root of the repository.'
        return
    
    colorizer = imp.load_source('svg_colorize', 'util/svg_colorize.py')
    
    modified = getModifiedIcons()
    customizations = getCustomizations()

    baseModifiedFiles = filterModified(modified, defaultCustomization)
    for customization in baseCustomizations:
        print 'Processing [%s]' % customization
        buildCustomization(customization, defaultCustomization, baseModifiedFiles) 

    customizations.remove(defaultCustomization)

    for customization in customizations:
        print 'Processing [%s]' % customization
        buildCustomization(customization, customization, filterModified(modified, customization))

if __name__ == "__main__":
    main()
