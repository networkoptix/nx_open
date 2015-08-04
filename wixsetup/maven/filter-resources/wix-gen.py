import os, sys, subprocess, shutil
from subprocess import Popen, PIPE
from os.path import dirname, join, exists, isfile

properties_dir='${root.dir}/wixsetup/${arch}'

#if not os.path.exists(properties_dir):
#    os.makedirs(properties_dir)
#os.system("echo ${install.type}=${finalName}.msi >> %s/installer.properties " % properties_dir)

generated_items = [, 'dbsync', 'help', 'vox', 'bg']

if '${nxtool}' == 'true':
    generated_items += ['qtquickcontrols']

for wxs in generated_items:
    p = subprocess.Popen('${init.python.dir}/python generate-%s-wxs.py' % wxs, shell=True, stdout=PIPE)
    out, err = p.communicate()
    print ('\n++++++++++++++++++++++Applying heat to generate-%s-wxs.py++++++++++++++++++++++' % wxs)
    print out
    p.wait()
    if p.returncode:  
        print "failed with code: %s" % str(p.returncode) 
        sys.exit(1)