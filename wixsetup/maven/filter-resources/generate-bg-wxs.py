import os, sys, subprocess
from subprocess import Popen, PIPE

if __name__ == '__main__':
    p = subprocess.Popen(r'heat dir ${ClientBgSourceDir} -wixvar -nologo -sfrag -suid -sreg -ag -srd -dir WebHelp -out ClientBg.wxs -cg ClientBgComponent -dr ${installer.customization}BgDir -var var.ClientBgSourceDir', shell=True, stdout=PIPE)
    out, err = p.communicate()
    print out
    p.wait()
    if p.returncode:  
        print "failed with code: %s" % str(p.returncode) 
        sys.exit(1)