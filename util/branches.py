#/bin/python

import subprocess
from datetime import datetime
from datetime import timedelta

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
            date = row[5:].strip()
            branch['date'] = datetime.strptime(date, "%Y-%m-%d")

result.sort(key=lambda branch: (branch['user'], branch['inactive']))

prevUser = ''

for branch in result:
    if prevUser != branch['user']:
        if prevUser != '':
            print '--------------------------------------------------------------------------------'
        prevUser = branch['user']
        print prevUser
    if branch['inactive']:
        print str(branch['name']).ljust(40), branch['date'], 'INACTIVE'
    else:
        delta = curDate - branch['date']
        if delta > timedelta(days = 30):
            print str(branch['name']).ljust(40), branch['date'], 'TOO OLD', delta.days, 'days'
        else:
            print str(branch['name']).ljust(40), branch['date']
