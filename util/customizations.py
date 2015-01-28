#/bin/python

import sys
import argparse
import os
from itertools import combinations

class Project:
    COMMON = 'common'
    CLIENT = 'client'
    ICONS = 'icons'
    INTRO = ["intro.mkv", "intro.avi", "intro.png", "intro.jpg", "intro.jpeg"]
    ALL = [COMMON, CLIENT, ICONS]

class Suffixes:
    HOVERED = '_hovered'
    SELECTED = '_selected'
    PRESSED = '_pressed'
    DISABLED = '_disabled'
    CHECKED = '_checked'
    ALL = [HOVERED, SELECTED, PRESSED, DISABLED, CHECKED]

    @staticmethod
    def baseName(fullname):
        result = fullname;
        for suffix in Suffixes.ALL:
            result = result.replace(suffix, "")
        return result

class Formats:
    PNG = '.png'
    GIF = '.gif'
    IMAGES = [PNG, GIF]

    CPP = '.cpp'
    H   = '.h'
    UI  = '.ui'
    SOURCES = [CPP, H, UI]

    @staticmethod
    def isImage(string):
        for format in Formats.IMAGES:
            if format in string:
                return True
        return False

    @staticmethod
    def isSource(string):
        for format in Formats.SOURCES:
            if string.endswith(format):
                return True
        return False

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

        with open(os.path.join(self.path, 'build.properties'), "r") as buildFile:
            for line in buildFile.readlines():
                if not 'parent.customization' in line:
                    continue
                self.parent = line.split('=')[1].strip()
                break
        
    def __str__(self):
        return self.name

    def isRoot(self):
        if not self.parent:
            err('Invalid build.properties file: ' + os.path.join(self.path, 'build.properties'))
            return False
        return self.parent == self.name
        
    def relativePath(self, path, entry):
        return os.path.relpath(os.path.join(path, entry), self.rootPath)
       
    def populateFileList(self):
    
        for dirname, dirnames, filenames in os.walk(self.basePath):
            cut = len(self.basePath) + 1
            for filename in filenames:
                norm = os.path.join(dirname, filename)[cut:].replace("\\", "/")
                self.base.append(norm)
        
        if hasattr(self, 'darkPath'):
            for dirname, dirnames, filenames in os.walk(self.darkPath):
                cut = len(self.darkPath) + 1
                for filename in filenames:
                    norm = os.path.join(dirname, filename)[cut:].replace("\\", "/")
                    self.dark.append(norm)
        
        if hasattr(self, 'lightPath'):
            for dirname, dirnames, filenames in os.walk(self.lightPath):
                cut = len(self.lightPath) + 1
                for filename in filenames:
                    norm = os.path.join(dirname, filename)[cut:].replace("\\", "/")
                    self.light.append(norm)
                
        self.total = sorted(list(set(self.base + self.dark + self.light)))
        
    def isUnused(self, entry, requiredFiles):
        if Suffixes.baseName(entry) in requiredFiles:
           return False
        if entry in Project.INTRO:
           return False
        if not Formats.isImage(entry):
           return False
        return True

    def validateInner(self, requiredFiles):
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
            if self.isUnused(entry, requiredFiles):
                clean = False
                warn('File %s in unused' % entry)

        for entry in self.dark:
            if not entry in self.light:
                clean = False
                if entry in self.base:
                    warn('File ' + self.relativePath(self.lightPath, entry) + ' missing, using base version')
                else:
                    error = True
                    err('File ' + self.relativePath(self.darkPath, entry) + ' missing in light skin')
            if self.isUnused(entry, requiredFiles):
                clean = False
                warn('File %s in unused' % entry)
                
        for entry in self.light:
            if not entry in self.dark:
                clean = False
                if entry in self.base:
                    warn('File ' + self.relativePath(self.darkPath, entry) + ' missing, using base version')
                else:
                    error = True
                    err('File ' + self.relativePath(self.lightPath, entry) + ' missing in dark skin')
            if self.isUnused(entry, requiredFiles):
                clean = False
                warn('File %s in unused' % entry)

        for entry in requiredFiles:
            if entry in self.base or entry in self.dark or entry in self.light:
                continue
            clean = False
            error = True
            err("File %s is missing" % entry)

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

def parseLine(line, extension, location):
    global verbose
    result = []
    splitter = '"'
    if extension == Formats.UI:
        line = line.replace("<", splitter).replace(">", splitter).replace(":/skin/", "")
        
    for part in line.split(splitter):
        if not Formats.isImage(part):
            continue
        result.append(part)
    return result

def parseFile(path):
    global verbose
    result = []
    linenumber = 0
    extension = os.path.splitext(path)[1]
    with open(path, "r") as file:
        for line in file.readlines():
            linenumber += 1
            if not Formats.isImage(line):
                continue
            if not "skin" in line and not "Skin" in line:
                if verbose:
                    warn(line.strip())
                continue
            result.extend(parseLine(line, extension, "%s:%s" % (path, linenumber)))
    return result

def parseSources(dir):
    result = []
    if not os.path.exists(dir):
        return result
    for entry in os.listdir(dir):
        path = os.path.join(dir, entry)
        if (os.path.isdir(path)):
            result.extend(parseSources(path))
            continue
        if not Formats.isSource(path):
            continue
        result.extend(parseFile(path))
    return result
        
def textCell(file, text, style = None):
    if style:
        file.write('\t<td class="%s">' % (style))
    else:
        file.write("\t<td>")
    file.write(text)
    file.write("</td>\n")

def imgCell(file, text, style):
    file.write('\t<td class="%s"><img src="%s"/></td>\n' % (style, text))

def printCustomizations(project, customizationDir, requiredFiles, customizations, roots, children):
    filename = os.path.join(customizationDir, project + ".html")
    file = open(filename, 'w')

    file.write("""<style>
img {max-width: 64px; }
td { text-align: center; }
td.label { text-align: left; }
td.dark { background-color: #222222; }
td.light { background-color: #D0D0D0; }
</style>\n""")
    file.write("<table>\n")
    file.write("<tr>\n")
    file.write("<th>File</th>\n")
    for c in roots:
        file.write("<th colspan=2>%s</th>\n" % c.name)
    for c in children:
        file.write("<th colspan=2>%s</th>\n" % c.name)
    file.write("</tr>\n")

    for entry in requiredFiles:
        file.write("<tr>\n")
        textCell(file, entry, "label")
        for c in roots:
            if entry in c.dark:
                imgCell(file, os.path.relpath(os.path.join(c.darkPath, entry), customizationDir), "dark")
            elif entry in c.base:
                imgCell(file, os.path.relpath(os.path.join(c.basePath, entry), customizationDir), "dark")
            else:
                textCell(file, "-")

            if entry in c.light:
                imgCell(file, os.path.relpath(os.path.join(c.lightPath, entry), customizationDir), "light")
            elif entry in c.base:
                imgCell(file, os.path.relpath(os.path.join(c.basePath, entry), customizationDir), "light")
            else:
                textCell(file, "-")

        for c in children:
            p = customizations[c.parent]
            if entry in c.dark:
                imgCell(file, os.path.relpath(os.path.join(c.darkPath, entry), customizationDir), "dark")
            elif entry in c.base:
                imgCell(file, os.path.relpath(os.path.join(c.basePath, entry), customizationDir), "dark")
            elif entry in p.dark:
                imgCell(file, os.path.relpath(os.path.join(p.darkPath, entry), customizationDir), "dark")
            elif entry in p.base:
                imgCell(file, os.path.relpath(os.path.join(p.basePath, entry), customizationDir), "dark")
            else:
                textCell(file, "-")

            if entry in c.light:
                imgCell(file, os.path.relpath(os.path.join(c.lightPath, entry), customizationDir), "light")
            elif entry in c.base:
                imgCell(file, os.path.relpath(os.path.join(c.basePath, entry), customizationDir), "light")
            elif entry in p.light:
                imgCell(file, os.path.relpath(os.path.join(p.lightPath, entry), customizationDir), "light")
            elif entry in p.base:
                imgCell(file, os.path.relpath(os.path.join(p.basePath, entry), customizationDir), "light")
            else:
                textCell(file, "-")

        file.write("</tr>\n")

    file.write("</table>")
    file.truncate()
    file.close()

def checkProject(rootDir, project):
    global separator
    info(separator)
    info('Checking project ' + project)
    info(separator)
    customizations = {}
    roots = []
    children = []
    invalidInner = 0

    sourcesDir = os.path.join(os.path.join(rootDir, project), 'src')
    requiredFiles = parseSources(sourcesDir)

    requiredSorted = sorted(list(set(requiredFiles)))
    customizationDir = os.path.join(rootDir, "customization")

    for entry in os.listdir(customizationDir):
        if (entry[:1] == '_'):
            continue 
        path = os.path.join(customizationDir, entry)
        if (not os.path.isdir(path)):
            continue
        c = Customization(entry, path, project)
        c.populateFileList()
        if c.isRoot():     
            invalidInner += c.validateInner(requiredSorted)                   
            roots.append(c)
        else:
            children.append(c)
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

    targetFiles = requiredSorted
    if len(targetFiles) == 0:
        targetFiles = customizations['default'].total

    printCustomizations(project, customizationDir, targetFiles, customizations, roots, children)

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
    sys.exit(0)