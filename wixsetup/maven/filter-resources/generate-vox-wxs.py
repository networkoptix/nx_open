import os, sys, subprocess
from subprocess import Popen, PIPE

if __name__ == '__main__':
    p = subprocess.Popen(r'heat dir ${ClientVoxSourceDir} -wixvar -nologo -sfrag -suid -sreg -ag -srd -dir WebHelp -out ClientVox.wxs -cg ClientVoxComponent -dr ${customization}VoxDir -var var.ClientVoxSourceDir', shell=True, stdout=PIPE)
    out, err = p.communicate()
    print out
    p.wait()
    if p.returncode:  
        print "failed with code: %s" % str(p.returncode) 
        sys.exit(1)
    p = subprocess.Popen(r'heat dir ${ClientVoxSourceDir} -wixvar -nologo -sfrag -suid -sreg -ag -srd -dir WebHelp -out ServerVox.wxs -cg ServerVoxComponent -dr ${customization}ServerVoxDir -var var.ClientVoxSourceDir', shell=True, stdout=PIPE)
    out, err = p.communicate()
    print out
    p.wait()
    if p.returncode:  
        print "failed with code: %s" % str(p.returncode) 
        sys.exit(1)        