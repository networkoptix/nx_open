import os, sys, subprocess, shutil
from subprocess import Popen, PIPE, STDOUT
from os.path import dirname, join, exists, isfile

properties_dir='${root.dir}/wixsetup/${arch}'

#if not os.path.exists(properties_dir):
#    os.makedirs(properties_dir)
#os.system("echo ${install.type}=${final.artifact.name}.msi >> %s/installer.properties " % properties_dir)

generated_items = [
    ('fonts', ''),
    ('qml', ''),
    ('help', ''),
    ('vox', ''),
    ('bg', ''),
    ('vcrt14', 'Client'),
    ('vcrt14', 'Server'),
    ('vcrt14', 'Traytool'),
    ('vcrt14', 'Nxtool')
]

if '${nxtool}' == 'true':
    generated_items += [('qtquickcontrols', '')]

for wxs, args in generated_items:
    p = subprocess.Popen('${init.python.dir}/python generate-{}-wxs.py {}'.format(wxs, args), shell=True, stdout=PIPE, stderr=STDOUT)
    out, err = p.communicate()
    print ('\n++++++++++++++++++++++Applying heat to generate-%s-wxs.py++++++++++++++++++++++' % wxs)
    print out
    p.wait()
    if p.returncode:
        print "failed with code: %s" % str(p.returncode)
        sys.exit(1)