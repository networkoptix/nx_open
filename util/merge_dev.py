#/bin/python

import subprocess
import sys

def execCommand(command):
    print command
    code = subprocess.call(command)
    if code != 0:
        sys.exit(code)
    return code
        
def getChangelog():
    command = 'hg log -b dev_2.3.1 -T "{desc}\\n\\n" -r "(::dev_2.3.1 - ::prod_2.3.1)"'
    changelog = subprocess.check_output(command, shell=True)
    changes = sorted(set(changelog.split('\n\n')))
    
    return '\n'.join(changes)
    
execCommand('hg up prod_2.3.1')     
execCommand('hg merge dev_2.3.1')
execCommand('hg ci -m"{0}"'.format(getChangelog()))
