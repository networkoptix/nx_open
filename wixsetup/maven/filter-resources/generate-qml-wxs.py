import os, sys, subprocess
from subprocess import Popen, PIPE

# Windows only
bin_source_dir = os.path.normcase('${bin_source_dir}')
qtdir = os.path.normcase('${qt.dir}')
qmldir = os.path.join(qtdir, 'qml')
qmldir_nxtool = os.path.join('${root.dir}/nxtool/static-resources/src', 'qml')

os.environ["PATH"] = "%s;%s\\bin" % (os.environ['PATH'], qtdir)

# Windows only so we can use Windows syntax here
commands = ['heat dir %s -wixvar -nologo -sfrag -suid -sreg -ag -srd -dir WebHelp -out ClientQml.wxs -cg ClientQmlComponent -dr ClientQml -var var.ClientQmlDir' % qmldir]

if __name__ == '__main__':
    cmd = '${qt.dir}\\bin\\windeployqt.exe %s\\desktop_client.exe --qmldir %s --no-translations --force --no-libraries --no-plugins --dir clientqml' % (bin_source_dir, qmldir)
    p = subprocess.Popen(cmd, shell=True, stdout=PIPE, stderr=subprocess.STDOUT)
    out, err = p.communicate()
    print out
    print err
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
        p = subprocess.Popen('${qt.dir}\\bin\\windeployqt.exe %s\\nxtool.exe --qmldir %s --no-translations --force --no-libraries --no-plugins --dir nxtoolqml' % (bin_source_dir, qmldir_nxtool), shell=True, stdout=PIPE)
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
