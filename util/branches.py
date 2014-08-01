#/bin/python

import subprocess
import sys
import argparse
from datetime import datetime
from datetime import timedelta

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('-a', '--all', action='store_true', help="show all branches (including valid)")
    args = parser.parse_args()
    all_branches = args.all
           
    output = subprocess.check_output("hg branches", shell=True)
    result = []
    curDate = datetime.now()
    tooOld = timedelta(days = 30)

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
        branch['valid'] = (branch['inactive'] == False) and (branch['age'] <= tooOld)

    result.sort(key=lambda branch: (branch['user'], branch['inactive']))

    prevUser = ''

    for branch in result:
        if (all_branches == False) and (branch['valid']):
            continue
            
        if prevUser != branch['user']:
            if prevUser != '':
                print '-------------------------------------------------------------------------------'
            prevUser = branch['user']
            print prevUser
        output = str(branch['name']).ljust(40)
        if branch['inactive']:
            output += '(INACTIVE)'

        if branch['age'] > tooOld:
            output += '(TOO OLD: ' + str(branch['age'].days) + ' days)'
        print output

if __name__ == "__main__":
    main()