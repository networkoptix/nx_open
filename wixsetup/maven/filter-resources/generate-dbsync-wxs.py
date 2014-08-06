import os, sys, subprocess
from subprocess import Popen, PIPE

if __name__ == '__main__':
    p = subprocess.Popen(r'heat dir ${DbSync22Dir} -wixvar -nologo -sfrag -suid -sreg -ag -srd -dir WebHelp -out DbSync22Files.wxs -cg DbSync22FilesComponent -dr ${customization}DbSync22Dir -var var.DbSync22SourceDir -wixvar', shell=True, stdout=PIPE)
    out, err = p.communicate()
    print out
    p.wait()
    if p.returncode:  
        print "failed with code: %s" % str(p.returncode) 
        sys.exit(1)
