#/bin/python

import subprocess
import sys
import os

base = 'traytool'
base_lang = 'en_US'

scriptDir = os.path.dirname(os.path.abspath(__file__))

projectFile = os.path.join(scriptDir, '../x64/' + base + '.pro')

baseEntry = ''
miscEntries = []

for entry in os.listdir(scriptDir):
    path = os.path.join(scriptDir, entry)
    
    if (os.path.isdir(path)):
        continue;
            
    if (not path[-2:] == 'ts'):
        continue;
        
    if (not entry.startswith(base)):
        continue;
        
    if (entry.find(base_lang) > 0):
        baseEntry = entry
    else:
        miscEntries.append(entry)
        

baseLangCommand = 'lupdate -no-obsolete -pro ' + projectFile + ' -locations absolute -pluralonly -ts ' + baseEntry
print baseLangCommand
subprocess.call(baseLangCommand)

miscLangCommand = 'lupdate -no-obsolete -pro ' + projectFile + ' -locations absolute -ts'
for lang in miscEntries:
    miscLangCommand = miscLangCommand + ' ' + lang
print miscLangCommand
subprocess.call(miscLangCommand)
