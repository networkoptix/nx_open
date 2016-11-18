import os, sys, subprocess

from string import Template
from subprocess import Popen, PIPE

if __name__ == '__main__':
    component = sys.argv[1]

    cgroup = '{}Vcrt14ComponentGroup'.format(component)
    dstdirvar = '{}Vcrt14DstDir'.format(component)
    outwxs = '{}Vcrt14.wxs'.format(component)

    template = Template(r'heat dir ${VC14RedistPath}\bin -wixvar -nologo -sfrag -suid -sreg -ag -srd -dir WebHelp -out $outwxs -cg $cgroup -dr $(var.$dstdirvar) -var var.Vcrt14SrcDir')
    command = template.safe_substitute({'outwxs': outwxs, 'cgroup': cgroup, 'dstdirvar': dstdirvar})

    p = subprocess.Popen(command, shell=True, stdout=PIPE)
    out, err = p.communicate()
    print out
    p.wait()
    if p.returncode:
        print "failed with code: %s" % str(p.returncode)
        sys.exit(1)


    # Add component_ prefix to avoid duplicating IDs for Server/Traytool
    lines = []
    with open(outwxs, 'r') as f:
        for line in f:
            lines.append(line
                        .replace('Component Id="', 'Component Id="{}_'.format(component))
                        .replace('ComponentRef Id="', 'ComponentRef Id="{}_'.format(component))
                        .replace('<File Id="', '<File Id="{}_'.format(component)))
    
    with open(outwxs, 'w') as f:
        f.write(''.join(lines))