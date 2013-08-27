import os, sys, posixpath
from os.path import dirname, join, exists, isfile
from os import listdir
import shutil
sys.path.insert(0, '${basedir}/../common')
#sys.path.insert(0, os.path.join('..', '..', 'common'))

template_file='template.pro'
specifics_file='${project.artifactId}-specifics.pro'
output_file='${project.artifactId}.pro'
translations_dir='${basedir}/translations'
translations_target_dir='${project.build.directory}/resources/translations'

def genqrc(qrcname, qrcprefix, pathes, extensions, exclusion, additions=''):
    os.path = posixpath

    qrcfile = open(qrcname, 'w')

    print >> qrcfile, '<!DOCTYPE RCC>'
    print >> qrcfile, '<RCC version="1.0">'
    print >> qrcfile, '<qresource prefix="%s">' % (qrcprefix)

    for path in pathes:
        for root, dirs, files in os.walk(path):
            parent = root[len(path) + 1:]
        
            for f in files:
                for extension in extensions:
                    if f.endswith(extension) and not f.endswith(exclusion):
                        print >> qrcfile, '<file alias="%s">%s</file>' % (os.path.join(parent, f), os.path.join(root, f))
  
    print >> qrcfile, additions
    print >> qrcfile, '</qresource>'
    print >> qrcfile, '</RCC>'
  
    qrcfile.close()

def gentext(file, path, extensions, text): 
    os.path = posixpath
   
    for root, dirs, files in os.walk(path):
        parent = root[len(path) + 1:]
        for f in files:
            f_short = os.path.splitext(f)[0]
            for extension in extensions:
                if f.endswith(extension) and not f_short.endswith('_specific'):
                #and not parent.endswith('_specific'):
                    print >> file, '\n%s%s/%s' % (text, path, os.path.join(parent, f))
                if f.endswith(extension) and f_short.endswith('${platform}_specific'):
                    print >> file, '\n%s%s/%s' % (text, path, os.path.join(parent, f))

def gen_includepath(file, path):      
    os.path = posixpath
    for dirs in os.walk(path).next()[1]:
        print >> file, '\nINCLUDEPATH += %s/%s' % (path, dirs)
                    
if __name__ == '__main__':
    os.system('mkdir build')
    if not os.path.exists(translations_target_dir):
        os.makedirs(translations_target_dir) 

    if os.path.exists(translations_dir):    
        for f in listdir(translations_dir):
            if f.endswith('.ts'):
                os.system('lrelease %s/%s -qm %s/%s.qm' % (translations_dir, f, translations_target_dir, os.path.splitext(f)[0]))
  
    genqrc('build/${project.artifactId}.qrc', '/', ['${project.build.directory}/resources','${libdir}/icons'], [''],'vmsclient.png')  
    
    if os.path.exists(os.path.join(r'${project.build.directory}', template_file)):
        f = open(output_file, "w")
        for file in [template_file, specifics_file]:
            if os.path.exists(file):
                fo = open(file, "r")
                f.write(fo.read())
                print >> f, '\n'
                fo.close()
        gentext(f, '${project.build.sourceDirectory}', ['.cpp', '.c'], 'SOURCES += ')
        gentext(f, '${project.build.sourceDirectory}', ['.h'], 'HEADERS += ')
        gentext(f, '${project.build.sourceDirectory}', ['.proto'], 'PB_FILES += ')
        gentext(f, '${project.build.sourceDirectory}', ['.ui'], 'FORMS += ')
        gen_includepath(f, '${libdir}/include')
        gen_includepath(f, '${environment.dir}/include')
        f.close()
    