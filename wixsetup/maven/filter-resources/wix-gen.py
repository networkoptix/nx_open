import os, sys, subprocess
from subprocess import Popen, PIPE

for wxs in ('dbsync', 'help', 'vox', 'bg'):
    p = subprocess.Popen('${python.dir}/python generate-%s-wxs.py' % wxs, shell=True, stdout=PIPE)
    out, err = p.communicate()
    print ('\n++++++++++++++++++++++Applying heat to generate-%s-wxs.py++++++++++++++++++++++' % wxs)
    print out
    p.wait()
    if p.returncode:  
        print "failed with code: %s" % str(p.returncode) 
        sys.exit(1)