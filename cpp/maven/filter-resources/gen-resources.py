import os, sys, posixpath
from os.path import dirname, join, exists, isfile
from os import listdir
sys.path.insert(0, '${basedir}/../common')
#sys.path.insert(0, os.path.join('..', '..', 'common'))

translations_dir='${basedir}/translations'
translations_target_dir='${project.build.directory}/resources/translations'

def genqrc(qrcname, qrcprefix, path, extensions, exclusion, additions=''):
  os.path = posixpath

  qrcfile = open(qrcname, 'w')

  print >> qrcfile, '<!DOCTYPE RCC>'
  print >> qrcfile, '<RCC version="1.0">'
  print >> qrcfile, '<qresource prefix="%s">' % (qrcprefix)

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


if __name__ == '__main__':
  os.system('mkdir build')
  if not os.path.exists(translations_target_dir):
    os.makedirs(translations_target_dir) 

  if os.path.exists(translations_dir):    
    for f in listdir(translations_dir):
      if f.endswith('.ts'):
        os.system('lrelease %s/%s -qm %s/%s.qm' % (translations_dir, f, translations_target_dir, os.path.splitext(f)[0]))
  
  genqrc('build/${project.artifactId}.qrc',           '/',        '${project.build.directory}/resources', [''],'.pdb')  