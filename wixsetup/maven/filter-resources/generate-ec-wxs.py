import os, sys, subprocess
from subprocess import Popen, PIPE

def fixasfiles():
    fragment1 = r"""            <Component Id="ecs.exe" Guid="*">
                <File Id="ecs.exe" KeyPath="yes" Source="$(var.AppServerSourceDir)\ecs.exe" />
            </Component>
"""

    fragment2 = """            <ComponentRef Id="ecs.exe" />
"""

    fragment3 = r"""            <Directory Id="db" Name="db">
                <Component Id="ecs.db" Guid="*">
                    <File Id="ecs.db" KeyPath="yes" Source="$(var.AppServerSourceDir)\db\ecs.db" />
                </Component>
            </Directory>
""" 

    fragment4 = """            <ComponentRef Id="ecs.db" />
"""

    text = open('AppServerFiles.wxs', 'r').read()
    text = text.replace(fragment1, '').replace(fragment2, '')
    text = text.replace(fragment3, '').replace(fragment4, '')
    open('AppServerFiles.wxs', 'w').write(text)

if __name__ == '__main__':
    p = subprocess.Popen(r'heat dir ${AppServerDir} -wixvar -nologo -sfrag -suid -sreg -ag -srd -dir WebHelp -out AppServerFiles.wxs -cg AppServerFilesComponent -dr ${customization}AppServerDir -var var.AppServerSourceDir -wixvar', shell=True, stdout=PIPE)
    out, err = p.communicate()
    print out
    p.wait()
    if p.returncode:  
        print "failed with code: %s" % str(p.returncode) 
        sys.exit(1)

    fixasfiles()