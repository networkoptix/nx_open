#!/usr/bin/env python
# -*- coding: utf-8 -*-

import os

PLATFORM = "${platform}"
ARCH = "${arch}"
BOX = "${box}"
BUILD_CONFIGURATION = "${build.configuration}"
TARGET_DIRECTORY = "${libdir}"
#RDEP_PATH = os.path.join(os.path.dirname(os.path.realpath("${environment.dir}")), "rdep")
RDEP_PATH = os.path.join("c:\\develop\\devtools", "rdep")

def make_list(string):
    return [v.strip() for v in string.split(',') if v]

def get_packages():
    packages = """${rdep.packages}"""
    if '${' in packages:
        return []
    return make_list(packages)
    
def print_configuration():
    print get_packages()
    print PLATFORM, ARCH, BOX, BUILD_CONFIGURATION
    print TARGET_DIRECTORY
    print RDEP_PATH
    
if __name__ == '__main__':   
    print_configuration()
