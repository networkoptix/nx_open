import os, sys, subprocess
from subprocess import Popen, PIPE

if '${arch}' == 'x64':
    out_folder = 'System64Folder'
else:
    out_folder = 'SystemFolder'
    
if __name__ == '__main__':
    p = subprocess.Popen(r'heat dir ${Vs2015crtDir} -wixvar -nologo -sfrag -suid -sreg -ag -srd -dir WebHelp -out vs2015crt.wxs -cg Vs2015crtComponent -dr {} -var var.Vs2015crtDir'.format(out_folder), shell=True, stdout=PIPE)
    out, err = p.communicate()
    print out
    p.wait()
    if p.returncode:  
        print "failed with code: %s" % str(p.returncode) 
        sys.exit(1)
