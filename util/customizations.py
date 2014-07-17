#/bin/python

import sys
import argparse
import os
from itertools import combinations

class ColorDummy():
    class Empty(object):
        def __getattribute__(self, name):
            return ''
    
    Style = Empty()
    Fore = Empty()

colorer = ColorDummy()

def info(message):
    print colorer.Style.BRIGHT + message
    
def green(message):
    print colorer.Style.BRIGHT + colorer.Fore.GREEN + message
        
def warn(message):
    print colorer.Style.BRIGHT + colorer.Fore.YELLOW + message
        
def err(message):
    print colorer.Style.BRIGHT + colorer.Fore.RED + message

class Customization():
    def __init__(self, name, path):
        self.name = name
        self.path = path
        skinPath = os.path.join(self.path, 'client/resources')
        self.basePath = os.path.join(skinPath, 'skin')
        self.darkPath = os.path.join(skinPath, 'skin_dark')
        self.lightPath = os.path.join(skinPath, 'skin_light')
        self.base = []
        self.dark = []
        self.light = []
        self.total = []
        
    def __str__(self):
        return self.name

    def isRoot(self):
        with open(os.path.join(self.path, 'build.properties'), "r") as buildFile:
            for line in buildFile.readlines():
                if not 'parent.customization' in line:
                    continue
                return (self.name in line)
        err('Invalid build.properties file: ' + os.path.join(self.path, 'build.properties'))
        return False
        
    def populateFileList(self):
    
        for dirname, dirnames, filenames in os.walk(self.basePath):
            cut = len(self.basePath) + 1
            for filename in filenames:
                self.base.append(os.path.join(dirname, filename)[cut:])
        
        for dirname, dirnames, filenames in os.walk(self.darkPath):
            cut = len(self.darkPath) + 1
            for filename in filenames:
                self.dark.append(os.path.join(dirname, filename)[cut:])
                
        for dirname, dirnames, filenames in os.walk(self.lightPath):
            cut = len(self.lightPath) + 1
            for filename in filenames:
                self.light.append(os.path.join(dirname, filename)[cut:])
                
        self.total = list(set(self.base + self.dark + self.light))
        
    def validateInner(self):
        info('Validating ' + self.name + '...')
        clean = True
        for entry in self.base:
            if entry in self.dark and entry in self.light:
                clean = False
                warn('File ' + os.path.join(self.basePath, entry) + ' duplicated in both skins')

        for entry in self.dark:
            if not entry in self.light:
                clean = False
                if entry in self.base:
                    warn('File ' + os.path.join(self.lightPath, entry) + ' missing, using base version')
                else:
                    err('File ' + os.path.join(self.darkPath, entry) + ' missing in light skin')
                
        for entry in self.light:
            if not entry in self.dark:
                clean = False
                if entry in self.base:
                    warn('File ' + os.path.join(self.darkPath, entry) + ' missing, using base version')
                else:
                    err('File ' + os.path.join(self.lightPath, entry) + ' missing in dark skin')
        if clean:
            green('Success')
        
    def validateCross(self, other):
        info('Validating ' + self.name + ' vs ' + other.name + '...')
        clean = True
        for entry in self.total:
            if not entry in other.total:
                clean = False
                err('File ' + os.path.join(self.basePath, entry) + ' missing in ' + other.name)
        if clean:
            green('Success')
        
def main():

    parser = argparse.ArgumentParser()
    parser.add_argument('-c', '--color', action='store_true', help="colorized output")
    args = parser.parse_args()
    if args.color:
        from colorama import Fore, Back, Style, init
        init(autoreset=True) # use Colorama to make Termcolor work on Windows too
        global colorer
        import colorama as colorer

    rootDir = '../customization';
    customizations = {}
    roots = []
    for entry in os.listdir(rootDir):
        if (entry[:1] == '_'):
            continue;   
        path = os.path.join(rootDir, entry)
        if (not os.path.isdir(path)):
            continue
        c = Customization(entry, path)
        if c.isRoot():
            c.populateFileList()
            c.validateInner()
            roots.append(c)
        customizations[entry] = c
    
    for c1, c2 in combinations(roots, 2):
        c1.validateCross(c2)
        c2.validateCross(c1)
    print colorer.Style.BRIGHT + 'Validation finished'

if __name__ == "__main__":
    main()
