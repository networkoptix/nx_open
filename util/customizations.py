#/bin/python

import sys
import argparse
import os
from itertools import combinations

class Project:
    COMMON = 'common'
    CLIENT = 'client'
    INTRO = ["intro.mkv", "intro.avi", "intro.png", "intro.jpg", "intro.jpeg"]
    ALL = [COMMON, CLIENT]

class Suffixes:
    HOVERED = '_hovered'
    SELECTED = '_selected'
    PRESSED = '_pressed'
    DISABLED = '_disabled'
    CHECKED = '_checked'

class ColorDummy():
    class Empty(object):
        def __getattribute__(self, name):
            return ''
    
    Style = Empty()
    Fore = Empty()

colorer = ColorDummy()
verbose = False
separator = '------------------------------------------------'

def info(message):
    if not verbose:
        return
    print colorer.Style.BRIGHT + message
    
def green(message):
    if not verbose:
        return
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
        self.rootPath = os.path.join(self.path, project)
        if project == Project.CLIENT:
            self.basePath = os.path.join(self.rootPath, 'resources', 'skin')
            self.darkPath = os.path.join(self.rootPath, 'resources', 'skin_dark')
            self.lightPath = os.path.join(self.rootPath, 'resources', 'skin_light')
        else:
            self.basePath = self.rootPath
            #self.darkPath = os.path.join(self.rootPath, 'resources', 'skin_dark')
            #self.lightPath = os.path.join(self.rootPath, 'resources', 'skin_light')
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
        return os.path.relpath(os.path.join(path, entry), self.rootPath)
       
    def populateFileList(self):
    
        for dirname, dirnames, filenames in os.walk(self.basePath):
            cut = len(self.basePath) + 1
            for filename in filenames:
                self.base.append(os.path.join(dirname, filename)[cut:])
        
        if hasattr(self, 'darkPath'):
            for dirname, dirnames, filenames in os.walk(self.darkPath):
                cut = len(self.darkPath) + 1
                for filename in filenames:
                    self.dark.append(os.path.join(dirname, filename)[cut:])
        
        if hasattr(self, 'lightPath'):
            for dirname, dirnames, filenames in os.walk(self.lightPath):
                cut = len(self.lightPath) + 1
                for filename in filenames:
                    self.light.append(os.path.join(dirname, filename)[cut:])
                
        self.total = sorted(list(set(self.base + self.dark + self.light)))
        
    def validateInner(self):
        info('Validating ' + self.name + '...')
        clean = True
        error = False
        
        if self.project == Project.CLIENT:
            hasIntro = False
            for intro in Project.INTRO:
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
            if self.project == Project.CLIENT:
                if entry in Project.INTRO:
                    continue
                
            if not entry in other.total:
                clean = False
                err('File ' + self.relativePath(self.basePath, entry) + ' missing in ' + other.name)
                
        if clean:
            green('Success')
            return 0
        return 1

def parseLine(line):
    result = []
    for part in line.split('"'):
        if not ".png" in part:
            continue
        result.append(part)
    return result

def parseFile(path):
    result = []
    with open(path, "r") as file:
        for line in file.readlines():
            if not ".png" in line:
                continue
            if not "skin" in line and not "Skin" in line:
                warn(line.strip())
                continue
            result.extend(parseLine(line))
    return result

def parseSources(dir):
    result = []
    for entry in os.listdir(dir):
        path = os.path.join(dir, entry)
        if (os.path.isdir(path)):
            result.extend(parseSources(path))
            continue
        if not path.endswith(".cpp"):
            continue
        result.extend(parseFile(path))
    return result
        
def checkProject(rootDir, project):
    global separator
    info(separator)
    info('Checking project ' + project)
    info(separator)
    customizations = {}
    roots = []
    invalidInner = 0

    sourcesDir = os.path.join(os.path.join(rootDir, project), 'src')
    requiredFiles = parseSources(sourcesDir)
  #  for file in requiredFiles:
  #      green(file)

    customizationDir = os.path.join(rootDir, "customization")

    for entry in os.listdir(customizationDir):
        if (entry[:1] == '_'):
            continue;   
        path = os.path.join(customizationDir, entry)
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

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('-c', '--color', action='store_true', help="colorized output")
    parser.add_argument('-v', '--verbose', action='store_true', help="verbose output")
    parser.add_argument('-p', '--project', default='', help="target project")
    args = parser.parse_args()
    if args.color:
        from colorama import Fore, Back, Style, init
        init(autoreset=True) # use Colorama to make Termcolor work on Windows too
        global colorer
        import colorama as colorer
    
    global verbose
    verbose = args.verbose

    projects = []
    if (args.project == Project.COMMON or args.project == Project.CLIENT):
        projects.append(args.project)
    else:
        projects = Project.ALL

    scriptDir = os.path.dirname(os.path.abspath(__file__))
    rootDir = os.path.join(scriptDir, '..')
    for project in projects:
        checkProject(rootDir, project)

if __name__ == "__main__":
    main()
