#/bin/python

import sys
import argparse
import os
from itertools import combinations

introNames = ["intro.mkv", "intro.avi", "intro.png", "intro.jpg", "intro.jpeg"]

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
    def __init__(self, name, path, project):
        self.name = name
        self.path = path
        self.project = project
        self.skinPath = os.path.join(self.path, project + '/resources')
        self.basePath = os.path.join(self.skinPath, 'skin')
        self.darkPath = os.path.join(self.skinPath, 'skin_dark')
        self.lightPath = os.path.join(self.skinPath, 'skin_light')
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
        
    def relativePath(self, path, entry):
        return os.path.relpath(os.path.join(path, entry), self.skinPath)
       
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
                
        self.total = sorted(list(set(self.base + self.dark + self.light)))
        
    def validateInner(self):
        info('Validating ' + self.name + '...')
        clean = True
        error = False
        
        if self.project == 'client':
            hasIntro = False
            for intro in introNames:
                if intro in self.total:
                    hasIntro = True
            
            if not hasIntro:
                clean = False
                error = True
                err('Intro is missing in ' + self.name)
        
        for entry in self.base:
            if entry in self.dark and entry in self.light:
                clean = False
                warn('File ' + os.path.join(self.basePath, entry) + ' duplicated in both skins')

        for entry in self.dark:
            if not entry in self.light:
                clean = False
                if entry in self.base:
                    warn('File ' + self.relativePath(self.lightPath, entry) + ' missing, using base version')
                else:
                    error = True
                    err('File ' + self.relativePath(self.darkPath, entry) + ' missing in light skin')
                
        for entry in self.light:
            if not entry in self.dark:
                clean = False
                if entry in self.base:
                    warn('File ' + self.relativePath(self.darkPath, entry) + ' missing, using base version')
                else:
                    error = True
                    err('File ' + self.relativePath(self.lightPath, entry) + ' missing in dark skin')
        if clean:
            green('Success')
        if error:
            return 1
        return 0
        
    def validateCross(self, other):
        info('Validating ' + self.name + ' vs ' + other.name + '...')
        clean = True
       
        for entry in self.total:
            #Intro files are checked an inner way
            if entry in introNames:
                continue
                
            if not entry in other.total:
                clean = False
                err('File ' + self.relativePath(self.basePath, entry) + ' missing in ' + other.name)
                
        if clean:
            green('Success')
            return 0
        return 1
        
def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('-c', '--color', action='store_true', help="colorized output")
    parser.add_argument('-p', '--project', default='client', help="target project, default is client")
    args = parser.parse_args()
    if args.color:
        from colorama import Fore, Back, Style, init
        init(autoreset=True) # use Colorama to make Termcolor work on Windows too
        global colorer
        import colorama as colorer
    
    project = args.project

    scriptDir = os.path.dirname(os.path.abspath(__file__))
    rootDir = os.path.join(scriptDir, '../customization')
    customizations = {}
    roots = []
    invalidInner = 0
    for entry in os.listdir(rootDir):
        if (entry[:1] == '_'):
            continue;   
        path = os.path.join(rootDir, entry)
        if (not os.path.isdir(path)):
            continue
        c = Customization(entry, path, project)
        if c.isRoot():
            c.populateFileList()
            invalidInner += c.validateInner()
            roots.append(c)
        customizations[entry] = c
    
    invalidCross = 0
    for c1, c2 in combinations(roots, 2):
        invalidCross += c1.validateCross(c2)
        invalidCross += c2.validateCross(c1)
    info('Validation finished')
    if invalidInner > 0:
        sys.exit(1)
    if invalidCross > 0:
        sys.exit(2)
    sys.exit(0)

if __name__ == "__main__":
    main()
