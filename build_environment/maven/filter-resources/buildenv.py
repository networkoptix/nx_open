import os, sys, platform, subprocess
from os import listdir
from os.path import dirname, join, exists, isfile
from pkg_resources import load_entry_point

basedir = join(dirname(os.path.abspath(__file__)))
sys.path.append(basedir + '/' + '../..')
from main import get_executable_extension, get_environment_variable, cd

basedir = join(dirname(os.path.abspath(__file__)))
environment = get_environment_variable('environment')

with cd(environment):
        status = subprocess.call('hg pull -u --clean', shell=True)
        if status != 0:
            sys.exit(status) 
            
with cd(environment):
        status = subprocess.call('%s/get_buildenv.%s ${arch} ${toolchain.version}' % (environment, get_executable_extension()), shell=True)
        if status != 0:
            sys.exit(status)             