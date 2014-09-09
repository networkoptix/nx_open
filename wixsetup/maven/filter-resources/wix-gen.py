import os, sys, subprocess
from subprocess import Popen, PIPE

bin_source_dir = ${libdir}/${arch}

for wxs in ('dbsync', 'help', 'vox', 'bg'):
    p = subprocess.Popen('${python.dir}/python generate-%s-wxs.py' % wxs, shell=True, stdout=PIPE)
    out, err = p.communicate()
    print ('\n++++++++++++++++++++++Applying heat to generate-%s-wxs.py++++++++++++++++++++++' % wxs)
    print out
    p.wait()
    if p.returncode:  
        print "failed with code: %s" % str(p.returncode) 
        sys.exit(1)
		
if os.path.exists(join(target_dir, '${applauncher.name}.exe')):
    os.shutil.rmtree(join(pdb_source_dir, '${applauncher.name}.exe'))
shutil.copy2(join(pdb_source_dir, 'applauncher.exe'), join(target_dir, '${applauncher.name}.exe'))
    
if os.path.exists(join(target_dir, '${client.name}.exe')):
    os.shutil.rmtree(join(pdb_source_dir, '${client.name}.exe'))          
shutil.copy2(join(pdb_source_dir, 'client.exe'), join(target_dir, '${client.name}.exe'))