import os, sys, subprocess
from subprocess import Popen, PIPE

# Windows only
qtdir = '${qt.dir}'.replace("/", "\\")
qmldir = os.path.join(qtdir, 'qml')

os.environ["PATH"] = "%s;%s\\bin" % (os.environ['PATH'], qtdir)
print os.environ["PATH"]

# Windows only so we can use Windows syntax here
commands = ['${environment.dir}\\bin\\windeployqt.exe ${libdir}\\${arch}\\bin\\${build.configuration}\client.bin.exe --qmldir %s --no-translations --force --no-libraries --no-plugins --dir qml' % qmldir,\
'del /f/q qml\QtWebProcess.exe'
'heat dir %s -wixvar -nologo -sfrag -suid -sreg -ag -srd -dir WebHelp -out ClientQml.wxs -cg ClientQmlComponent -dr ClientQml -var var.ClientQmlDir' % qmldir
]

if __name__ == '__main__':
    p = subprocess.Popen('${environment.dir}\\bin\\windeployqt.exe ${libdir}\\${arch}\\bin\\${build.configuration}\client.bin.exe --qmldir %s --no-translations --force --no-libraries --no-plugins --dir clientqml' % qmldir, shell=True, stdout=PIPE)
    out, err = p.communicate()
    print out
    p.wait()
    if p.returncode != 0 and p.returncode != 204:  
        print "failed with code: %s" % str(p.returncode) 
        sys.exit(1)
    if os.path.exists('clientqml/QtWebProcess.exe'):
        os.unlink('clientqml/QtWebProcess.exe')

    p = subprocess.Popen('heat dir clientqml -wixvar -nologo -sfrag -suid -sreg -ag -srd -dir WebHelp -out ClientQml.wxs -cg ClientQmlComponent -dr ClientQml -var var.ClientQmlDir', shell=True, stdout=PIPE)
    out, err = p.communicate()
    print out
    p.wait()
    if p.returncode != 0 and p.returncode != 204:  
        print "failed with code: %s" % str(p.returncode) 
        sys.exit(1)

    if os.path.exists('nxtoolqml/QtWebProcess.exe'):
        os.unlink('nxtoolqml/QtWebProcess.exe')
        
    if '${nxtool}' == 'true':
        p = subprocess.Popen('${environment.dir}\\bin\\windeployqt.exe ${libdir}\\${arch}\\bin\\${build.configuration}\\nxtool.exe --qmldir %s --no-translations --force --no-libraries --no-plugins --dir nxtoolqml' % qmldir, shell=True, stdout=PIPE)
        out, err = p.communicate()
        print out
        p.wait()
        if p.returncode != 0 and p.returncode != 204:  
            print "failed with code: %s" % str(p.returncode) 
            sys.exit(1)    
        
        p = subprocess.Popen('heat dir nxtoolqml -wixvar -nologo -sfrag -suid -sreg -ag -srd -dir WebHelp -out NxtoolQml.wxs -cg NxtoolQmlComponent -dr NxtoolQml -var var.NxtoolQmlDir', shell=True, stdout=PIPE)
        out, err = p.communicate()
        print out
        p.wait()
        if p.returncode != 0 and p.returncode != 204:  
            print "failed with code: %s" % str(p.returncode) 
            sys.exit(1)