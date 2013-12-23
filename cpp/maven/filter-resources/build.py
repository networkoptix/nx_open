import os, sys, platform, subprocess
from os import listdir
from os.path import dirname, join, exists, isfile
from pkg_resources import load_entry_point

basedir = join(dirname(os.path.abspath(__file__)))

if __name__ == '__main__':            
    if '${platform}' != 'windows':
        os.system('chmod 777 build.sh')
        status = subprocess.call('%s/build.sh' % basedir, shell=True)
        if status != 0:
            sys.exit(status)             
    else:
        status = subprocess.call('%s/build.bat' % basedir, shell=True)
        if status != 0:
            sys.exit(status)      