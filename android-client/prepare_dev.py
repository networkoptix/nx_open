# -*- coding: utf-8 -*-
#/bin/python

import os
import shutil

def copyResources(rootDir):
    print "Copying resources..."
    sourceDir = os.path.join(rootDir, 'target', 'resources')
    targetDir = os.path.join(rootDir, 'res')
    shutil.rmtree(targetDir, ignore_errors=True)
    shutil.move(sourceDir, targetDir)  
    
def cleanLibraries(rootDir):
    print "Clearing libraries..."
    libDir = os.path.join(rootDir, 'libs')
    libsToRemove = ['android-4.4.2.jar']
    for lib in libsToRemove:
        libPath = os.path.join(libDir, lib)
        os.remove(libPath)
    
def main():
    scriptDir = os.path.dirname(os.path.abspath(__file__))   
    rootDir = os.path.join(scriptDir, 'android-main')

    copyResources(rootDir)
    cleanLibraries(rootDir)
    print 'ok'
    
if __name__ == "__main__":
    main()
    
