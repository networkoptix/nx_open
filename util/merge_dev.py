# -*- coding: utf-8 -*-
#/bin/python

import subprocess
import sys
import os
import argparse

def execCommand(command):
    print command
    code = subprocess.call(command)
    if code != 0:
        sys.exit(code)
    return code
        
def getChangelog(revision):
    command = 'hg log -T "{desc}\\n\\n" -r '
    changeset = '"(::{0} - ::prod_2.3.1)"'.format(revision)
    command = command + changeset
    print command
    changelog = subprocess.check_output(command, shell=True)
    changes = sorted(set(changelog.split('\n\n')))
    
    return '\n'.join(changes)
      
def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('-r', '--rev', type=str, help="Target revision")
    parser.add_argument('-p', '--preview', action='store_true', help="preview changes")
    args = parser.parse_args()
    
    revision = args.rev
    if not revision:
        revision = 'dev_2.3.1'
   
    if args.preview:
        print getChangelog(revision)
        sys.exit(0)
        
    execCommand('hg up prod_2.3.1')     
    execCommand('hg merge {0}').format(revision)
    execCommand('hg ci -m"{0}"'.format(getChangelog()))
    sys.exit(0)
    
if __name__ == "__main__":
    main()
