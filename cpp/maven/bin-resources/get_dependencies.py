#!/usr/bin/env python
# -*- coding: utf-8 -*-

import os, sys, shutil
import dependencies

sys.path.insert(0, dependencies.RDEP_PATH)
import rdep
sys.path.pop(0)

debug = "debug" in dependencies.BUILD_CONFIGURATION

def fetch_packages(packages):    
    return rdep.fetch_packages(packages, dependencies.TARGET, debug, True)
    
    
def copy_package(package):
    path = rdep.locate_package(package, dependencies.TARGET, debug)
    if not path:
        print "Could not locate {0}".format(package)
        return False
    
'''
TODO:
0) check if artifact is already processed (copy rdpack to qt-5.6.0.rdpack)
1) read copy-target list from artifact, by default - bin subdirectory
2) copy only selected resources to the target dir(s?), e.g. TARGET_DIRECTORY\x64\bin\debug
'''
    target_dir = dependencies.TARGET_DIRECTORY
    print "Copying package {0} into {1}".format(package, target_dir)
    for dirname, dirnames, filenames in os.walk(path):
        rel_dir = os.path.relpath(dirname, path)
        dir = os.path.abspath(os.path.join(target_dir, rel_dir))
        if not os.path.isdir(dir):
            os.makedirs(dir)               
        for filename in filenames:
            entry = os.path.join(dirname, filename)
            shutil.copy(entry, dir)
    return True

    
def copy_packages(packages):
    return all([copy_package(package) for package in packages])  
    
    
def get_dependencies():
    packages = dependencies.get_packages()
    if not packages:    
        print "No dependencies found"
        return
        
    dependencies.print_configuration()
    
    if not fetch_packages(packages):
        print "Cannot fetch packages"
        sys.exit(1)
    
    if not copy_packages(packages):
        print "Cannot copy packages"
        sys.exit(1)
    
    
if __name__ == '__main__':   
    get_dependencies()