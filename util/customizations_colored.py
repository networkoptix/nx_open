#/bin/python

import sys
import os
from colorama import Fore, Back, Style, init

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
        
    def __str__(self):
        return self.name

    def isRoot(self):
        with open(os.path.join(self.path, 'build.properties'), "r") as buildFile:
            for line in buildFile.readlines():
                if not 'parent.customization' in line:
                    continue
                return (self.name in line)
        print Style.BRIGHT + Fore.RED + 'Invalid build.properties file: ' + os.path.join(self.path, 'build.properties')
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
        
    def validateInner(self):
        print Style.BRIGHT + 'Validating ' + self.name + '...'
        for entry in self.base:
            if entry in self.dark:
                print Style.BRIGHT + Fore.YELLOW + 'File ' + os.path.join(self.basePath, entry) + ' duplicated in dark skin'
            if entry in self.light:
                print Style.BRIGHT + Fore.YELLOW + 'File ' + os.path.join(self.basePath, entry) + ' duplicated in light skin'
        for entry in self.dark:
            if not entry in self.light:
                print Style.BRIGHT + Fore.RED + 'File ' + os.path.join(self.darkPath, entry) + ' missing in light skin'
                
        for entry in self.light:
            if not entry in self.dark:
                print Style.BRIGHT + Fore.RED + 'File ' + os.path.join(self.lightPath, entry) + ' missing in dark skin'
                
        
    def validateCross(self, other):
        print Style.BRIGHT + 'Validating ' + self.name + ' vs ' + other.name + '...'
        for entry in self.base:
            if not entry in other.base:
                print Style.BRIGHT + Fore.RED + 'File ' + os.path.join(self.basePath, entry) + ' missing in ' + other.basePath

        for entry in self.dark:
            if not entry in other.dark:
                print Style.BRIGHT + Fore.RED + 'File ' + os.path.join(self.darkPath, entry) + ' missing in ' + other.darkPath
                
        for entry in self.light:
            if not entry in other.light:
                print Style.BRIGHT + Fore.RED + 'File ' + os.path.join(self.lightPath, entry) + ' missing in ' + other.lightPath
        
def main():
    # use Colorama to make Termcolor work on Windows too
    init(autoreset=True)

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
    
    for n, c1 in enumerate(roots):
        for c2 in roots[n+1:]:
            c1.validateCross(c2)
            c2.validateCross(c1)
 

if __name__ == "__main__":
    main()