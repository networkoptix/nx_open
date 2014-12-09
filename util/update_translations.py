#/bin/python

import sys
import os
import argparse
import threading
import subprocess

projects = ['common', 'client', 'traytool']

def update(project, translationDir, projectFile):
    entries = []

    for entry in os.listdir(translationDir):
        path = os.path.join(translationDir, entry)
        
        if (os.path.isdir(path)):
            continue;
                
        if (not path[-2:] == 'ts'):
            continue;
            
        if (not entry.startswith(project)):
            continue;
            
        entries.append(entry)
            
    command = 'lupdate -no-obsolete -pro ' + projectFile + ' -locations absolute -ts'
    for lang in entries:
        command = command + ' ' + lang
    print command
    subprocess.call(command)

def main():
    scriptDir = os.path.dirname(os.path.abspath(__file__))   
    rootDir = os.path.join(scriptDir, '..')
    
    threads = []
    for project in projects:
        projectDir = os.path.join(rootDir, project)
        translationDir = os.path.join(projectDir, 'translations')
        projectFile = os.path.join(projectDir, 'x64/' + project + '.pro')
        thread = threading.Thread(None, update, args=(project, translationDir, projectFile))
        thread.start()
        threads.append(thread)
        
    for thread in threads:
        thread.join()
        
    print "ok"
    
    
if __name__ == "__main__":
    main()