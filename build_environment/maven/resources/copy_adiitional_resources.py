import io, sys, os, errno, platform, shutil
from os.path import dirname, join, exists, isfile
#from main import get_environment_variable, cd

qtlibs = ['core', 'gui', 'network', 'opengl', 'xml', 'multimedia', 'sql', '']

def mkdir_p(path):
    try:
        os.makedirs(path)
    except OSError as exc: # Python >2.5
        if exc.errno == errno.EEXIST and os.path.isdir(path):
            pass
        else: raise

for x in ('x86', 'x64'):
    for d in ('debug', 'release'):
        if not os.path.exists(join('c:\\develop\\projects\\netoptix_vms\\build_environment\\target', x, 'bin', d)):
            mkdir_p(join('c:\\develop\\projects\\netoptix_vms\\build_environment\\target', x, 'bin', d))

#for qtlib in qtlibs:
#    if qtlib != '':
        