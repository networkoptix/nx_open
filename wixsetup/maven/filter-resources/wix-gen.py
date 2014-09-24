import os, sys, subprocess, shutil
from subprocess import Popen, PIPE
from os.path import dirname, join, exists, isfile

bin_source_dir = '${libdir}/${arch}/bin/${build.configuration}'

for wxs in ('dbsync', 'help', 'vox', 'bg'):
    p = subprocess.Popen('${python.dir}/python generate-%s-wxs.py' % wxs, shell=True, stdout=PIPE)
    out, err = p.communicate()
    print ('\n++++++++++++++++++++++Applying heat to generate-%s-wxs.py++++++++++++++++++++++' % wxs)
    print out
    p.wait()
    if p.returncode:  
        print "failed with code: %s" % str(p.returncode) 
        sys.exit(1)
		
if os.path.exists(join(bin_source_dir, '${product.name} Launcher.exe')):
    os.unlink(join(bin_source_dir, '${product.name} Launcher.exe'))
if os.path.exists(join(bin_source_dir, 'applauncher.exe')):
    shutil.copy2(join(bin_source_dir, 'applauncher.exe'), join(bin_source_dir, '${product.name} Launcher.exe'))
    
if os.path.exists(join(bin_source_dir, '${product.name}.exe')):
    os.unlink(join(bin_source_dir, '${product.name}.exe'))          
if os.path.exists(join(bin_source_dir, 'client.exe')):
    shutil.copy2(join(bin_source_dir, 'client.exe'), join(bin_source_dir, '${product.name}.exe'))