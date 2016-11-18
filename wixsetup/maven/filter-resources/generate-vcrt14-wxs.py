import os, sys, subprocess
from subprocess import Popen, PIPE

if __name__ == '__main__':
    p = subprocess.Popen(r'heat dir ${VC14RedistPath}\bin -wixvar -nologo -sfrag -suid -sreg -ag -srd -dir WebHelp -out Vcrt14.wxs -cg Vcrt14ComponentGroup -dr $(var.Vcrt14DstDir) -var var.Vcrt14SrcDir', shell=True, stdout=PIPE)
    out, err = p.communicate()
    print out
    p.wait()
    if p.returncode:
        print "failed with code: %s" % str(p.returncode)
        sys.exit(1)