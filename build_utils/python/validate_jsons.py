#/bin/python

import sys
import os
import argparse
import threading
import subprocess
from subprocess import Popen, PIPE
from os.path import dirname, join, exists, isfile
from fnmatch import fnmatch

def main():
    root = os.getcwd()
    pattern = "*.json"
       
    for path, subdirs, files in os.walk(root):
        for name in files:
            if fnmatch(name, pattern) and name != 'globals.json' and name != 'serverProperties.json':
                p = subprocess.Popen('jsonlint -v %s' % os.path.join(path, name), shell=True, stdout=PIPE)
                out, err = p.communicate()
                print out
                p.wait()
                if p.returncode:
                    print "failed with code: %s" % str(p.returncode) 
                    sys.exit(1)
                
if __name__ == "__main__":
    if sys.platform == 'linux2':
        main()