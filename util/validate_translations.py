# -*- coding: utf-8 -*-
#/bin/python

import sys
import os
import argparse
import xml.etree.ElementTree as ET

projects = ['common', 'client', 'traytool']

critical = ['\t', '%1', '%2', '%3', '%4', '%5', '%6', '%7', '%8', '%9']
warned = ['%n', '\n']

class ColorDummy():
    class Empty(object):
        def __getattribute__(self, name):
            return ''
    
    Style = Empty()
    Fore = Empty()

colorer = ColorDummy()

def info(message):
    print colorer.Style.BRIGHT + message.encode('utf-8')
    
def green(message):
    print colorer.Style.BRIGHT + colorer.Fore.GREEN + message.encode('utf-8')
        
def warn(message):
    print colorer.Style.BRIGHT + colorer.Fore.YELLOW + message.encode('utf-8')
        
def err(message):
    print colorer.Style.BRIGHT + colorer.Fore.RED + message.encode('utf-8')

class ValidationResult():
    error = 0
    warned = 0
    unfinished = 0
    total = 0

def symbolText(symbol):
    if symbol == '\t':
        return '\\t'
    if symbol == '\n':
        return '\\n'
    return symbol

def checkText(source, target, location, result, verbose):

    filename = location.get('filename') if location is not None else 'unknown'
    line = location.get('line') if location is not None else 'unknown'

    for symbol in critical:
        occurences = source.count(symbol)
        if target.count(symbol) != occurences:
            err(u'Invalid translation string, error on {0} count:\nLocation: {1} line {2}\nSource: {3}\nTarget: {4}'
                .format(
                    symbolText(symbol),
                    filename, line,
                    source, target))
            result.error += 1
            break

    if verbose:
        for symbol in warned:
            occurences = source.count(symbol)
            if target.count(symbol) != occurences:
                warn(u'Invalid translation string, error on {0} count:\nLocation: {1} line {2}\nSource: {3}\nTarget: {4}'
                    .format(
                        symbolText(symbol),
                        filename, line,
                        source, target))
                result.warned += 1
                break

    return result;

def validateXml(root, verbose):
    result = ValidationResult()

    for context in root:
        for message in context.iter('message'):
            result.total += 1
            source = message.find('source')
            translation = message.find('translation')
            location = message.find('location')
            if translation.get('type') == 'unfinished':
                result.unfinished += 1
                continue

            #Using source text
            if not translation.text:
                continue

            hasNumerusForm = False
            for numerusform in translation.iter('numerusform'):
                hasNumerusForm = True
                if not numerusform.text:
                    continue;
                result = checkText(source.text, numerusform.text, location, result, verbose)

            if not hasNumerusForm:
                result = checkText(source.text, translation.text, location, result, verbose)

    return result

def validate(path, verbose):
    name = os.path.basename(path)
    if verbose:
        info('Validating {0}...'.format(name))
    tree = ET.parse(path)
    root = tree.getroot()
    result = validateXml(root, verbose)

    if result.error > 0:
        err('{0}: {1} errors found'.format(name, result.error))

    if not verbose:
        return

    if result.unfinished > 0:
        warn('{0}: {1} of {2} translations are unfinished'.format(name, result.unfinished, result.total))
    else:
        green('{0}: ok'.format(name))

def validateProject(project, translationDir, verbose):
    entries = []

    for entry in os.listdir(translationDir):
        path = os.path.join(translationDir, entry)
        
        if (os.path.isdir(path)):
            continue;
                
        if (not path[-2:] == 'ts'):
            continue;
            
        if (not entry.startswith(project)):
            continue;
            
        entries.append(path)
            
    count = 0
    for path in entries:
        validate(path, verbose)

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('-c', '--color', action='store_true', help="colorized output")
    parser.add_argument('-v', '--verbose', action='store_true', help="verbose output")
    args = parser.parse_args()
    if args.color:
        from colorama import Fore, Back, Style, init
        init(autoreset=True) # use Colorama to make Termcolor work on Windows too
        global colorer
        import colorama as colorer

    scriptDir = os.path.dirname(os.path.abspath(__file__))   
    rootDir = os.path.join(scriptDir, '..')
    
    for project in projects:
        projectDir = os.path.join(rootDir, project)
        translationDir = os.path.join(projectDir, 'translations')
        validateProject(project, translationDir, args.verbose)
       
    if args.verbose:
        info("Validation finished.")
    
    
if __name__ == "__main__":
    main()