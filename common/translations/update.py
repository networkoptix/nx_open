#/bin/python

import subprocess
import sys
import os

base = 'common'

scriptDir = os.path.dirname(os.path.abspath(__file__))

projectFile = os.path.join(scriptDir, '../x64/' + base + '.pro')

entries = []

for entry in os.listdir(scriptDir):
    path = os.path.join(scriptDir, entry)
    
    if (os.path.isdir(path)):
        continue;
            
    if (not path[-2:] == 'ts'):
        continue;
        
    if (not entry.startswith(base)):
        continue;
        
    entries.append(entry)
        
command = 'lupdate -no-obsolete -pro ' + projectFile + ' -locations absolute -ts'
for lang in entries:
    command = command + ' ' + lang
print command
subprocess.call(command)
