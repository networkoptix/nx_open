import os, sys, subprocess, shutil
from subprocess import Popen, PIPE

commands = [
'candle -dinstalltype="${install.type}" -arch ${arch} -out obj\${build.configuration}\ Product-${install.type}.wxs -ext WixBalExtension',
'light -cultures:${installer.language} -loc CustomStrings_${installer.language}.wxl  -out bin/${finalName}.msi obj\${build.configuration}\*.wixobj -ext WixBalExtension',
'cscript FixExitDialog.js bin/${finalName}.msi'
]

commands = [
    os.path.join(os.getenv('environment'), "is5/ISCC.exe") + ' /obin /f${project.build.finalName} Product-full.iss'
]

for command in commands:
    p = subprocess.Popen(command, shell=True, stdout=PIPE)
    out, err = p.communicate()
    print ('\n++++++++++++++++++++++%s++++++++++++++++++++++' % command)
    print out
    p.wait()
    if p.returncode != 0 and p.returncode != 204:  
        print "failed with code: %s" % str(p.returncode) 
        sys.exit(1)
    
#for files in os.listdir('${project.build.directory}/bin/'):
#    if files.endswith(".msi"):
#        shutil.move(os.path.join('${project.build.directory}/bin/', files), '${project.build.directory}')