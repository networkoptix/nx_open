# -*- coding: utf-8 -*-
#/bin/python

import subprocess
import sys
import argparse
from datetime import datetime
from datetime import timedelta
from itertools import groupby
from common_module import init_color,info,green,warn,err,separator

tooOld = timedelta(days = 30)
verbose = False

class Branch():
    def __init__(self, name, rev, hash, active):
        self.name = name
        self.rev = rev
        self.hash = hash
        self.active = active
        self.user = ''
        self.date = datetime.now()
        self.age = self.date - self.date
        
    def __str__(self):
        return self.name

def check_branches():
    output = subprocess.check_output("hg branches", shell=True)
    result = []
    curDate = datetime.now()

    for row in output.split('\n'):
        if ':' in row:
            key, hash = row.split(':')
            name, rev = key.split()
            if name == 'default':
                continue
            branch = Branch(name, rev, hash, True)
            branch = Branch(name, rev, hash, True)
            if '(' in hash:
                branch.hash = hash.split('(')[0].strip()
                branch.active = False
            result.append(branch)

    for branch in result:
        log = "hg log -r " + branch.rev + ' --template "date:{date|shortdate}\\nuser:{author}"'
        info = subprocess.check_output(log, shell=True)
        for row in info.split('\n'):
            if 'user:' in row:
                key, value = row.split(':')
                user = value.strip()
                if '<' in user:
                    user = user.split('<')[0].strip()
                branch.user = user
            if 'date:' in row:
                dateStr = row[5:].strip()
                date = datetime.strptime(dateStr, "%Y-%m-%d")
                age = curDate - date
                branch.date = date
                branch.age = age
  
    userKey = lambda branch: branch.user
    result.sort(key=userKey)
    return groupby(result, userKey)

def print_branches(grouped_branches):
    prevUser = ''

    for user, branches_iter in grouped_branches:           
        branches = [i for i in branches_iter if verbose or not i.active or i.age > tooOld]
        if len(branches) == 0:
            continue
            
        if prevUser != '':
            separator()
        prevUser = user
        info(user)
        for branch in sorted(branches, key = lambda x: not x.active):
            branch_name = str(branch.name).ljust(40)
            if not branch.active:
                err(branch_name + '(INACTIVE)')
            elif branch.age > tooOld:
                warn(branch_name + '(TOO OLD: ' + str(branch.age.days) + ' days)')
            elif verbose == True:
                info(branch_name)
    
def main(): 
    parser = argparse.ArgumentParser()
    parser.add_argument('-v', '--verbose', action='store_true', help="verbose output")
    parser.add_argument('-c', '--color', action='store_true', help="colorized output")
    args = parser.parse_args()
    global verbose
    verbose = args.verbose

    if args.color:
        init_color()    
           
    branches = check_branches()
    print_branches(branches)

if __name__ == "__main__":
    main()