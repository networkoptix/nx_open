#!/usr/bin/env python
# -*- coding: utf-8 -*-

import os, sys
import dependencies

sys.path.insert(0, dependencies.RDEP_PATH)
import rdep
sys.path.pop(0)

def fetch_packages(packages):
    debug = "debug" in dependencies.BUILD_CONFIGURATION
    return rdep.fetch_packages(packages, dependencies.PLATFORM, dependencies.ARCH, dependencies.BOX, debug, True)
    
def get_dependencies():
    packages = dependencies.get_packages()
    if not packages:    
        print "No dependencies found"
        return
        
    dependencies.print_configuration()
    
    if not fetch_packages(packages):
        print "Cannot fetch packages"
        sys.exit(1)
    
    
    
if __name__ == '__main__':   
    get_dependencies()