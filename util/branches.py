# -*- coding: utf-8 -*-
#/bin/python

import subprocess
import sys
import argparse
from datetime import datetime
from datetime import timedelta
from common_module import init_color,info,green,warn,err,separator

tooOld = timedelta(days = 30)
verbose = False

def check_branches():
    output = subprocess.check_output("hg branches", shell=True)
    result = []
    curDate = datetime.now()

    for row in output.split('\n'):
        if ':' in row:
            key, value = row.split(':')
            name, rev = key.split()
            if name == 'default':
                continue
            branch = {'name' : name, 'rev' : rev, 'hash': value, 'inactive': False }
            if '(' in value:
                branch['hash'] = value.split('(')[0].strip()
                branch['inactive'] = True
            result.append(branch)

    for branch in result:
        log = "hg log -r " + branch['rev'] + ' --template "date:{date|shortdate}\\nuser:{author}"'
        info = subprocess.check_output(log, shell=True)
        for row in info.split('\n'):
            if 'user:' in row:
                key, value = row.split(':')
                user = value.strip()
                if '<' in user:
                    user = user.split('<')[0].strip()
                branch['user'] = user
            if 'date:' in row:
                dateStr = row[5:].strip()
                date = datetime.strptime(dateStr, "%Y-%m-%d")
                age = curDate - date
                branch['date'] = date
                branch['age'] = age

    result.sort(key=lambda branch: (branch['user'], branch['inactive']))
    return result

def print_branches(branches):
    prevUser = ''

    for branch in branches:           
        if prevUser != branch['user']:
            if prevUser != '':
                separator()
            prevUser = branch['user']
            info(prevUser)
            separator()
        branch_name = str(branch['name']).ljust(40)
        if branch['inactive']:
            err(branch_name + '(INACTIVE)')
        elif branch['age'] > tooOld:
            warn(branch_name + '(TOO OLD: ' + str(branch['age'].days) + ' days)')
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