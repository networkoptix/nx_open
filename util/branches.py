#/bin/python

import subprocess
output = subprocess.check_output("hg branches", shell=True)
result = []

for row in output.split('\n'):
    if ':' in row:
        key, value = row.split(':')
        name, rev = key.split()
        branch = {'name' : name, 'rev' : rev, 'hash': value, 'inactive': False }
        if '(' in value:
            branch['hash'] = value.split('(')[0].strip()
            branch['inactive'] = True
        result.append(branch)

for branch in result:
    info = subprocess.check_output("hg log -r " + branch['rev'], shell=True)
    for row in info.split('\n'):
        if 'user:' in row:
            key, value = row.split(':')
            user = value.strip()
            branch['user'] = user
        if 'date:' in row:
            date = row[5:].strip()
            branch['date'] = date

result.sort(key=lambda branch: (branch['user'], branch['inactive']))

prevUser = ''
for branch in result:
    if prevUser != branch['user']:
        prevUser = branch['user']
        print '----------------------------------------'
        print prevUser
    if branch['inactive']:
        print branch['name'], '\t', branch['date'], 'INACTIVE'
    else:
        print branch['name'], '\t', branch['date']
