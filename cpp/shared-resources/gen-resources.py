import os, sys, posixpath
sys.path.insert(0, '${basedir}/../common')
#sys.path.insert(0, os.path.join('..', '..', 'common'))

from gencomp import gencomp_cpp
#os.makedirs('${project.build.directory}/build/release/generated/')
gencomp_cpp(open('${project.build.sourceDirectory}/compatibility_info.cpp', 'w'))

def platform():
    if sys.platform == 'win32':
        return 'win32'
    elif sys.platform == 'darwin':
        return 'mac'
    elif sys.platform == 'linux2':
        return 'linux'

def genqrc(qrcname, qrcprefix, path, extensions, additions=''):
  os.path = posixpath

  qrcfile = open(qrcname, 'w')

  print >> qrcfile, '<!DOCTYPE RCC>'
  print >> qrcfile, '<RCC version="1.0">'
  print >> qrcfile, '<qresource prefix="%s">' % (qrcprefix)

  for root, dirs, files in os.walk(path):
    parent = root[len(path) + 1:]
    if '.svn' in dirs:
      dirs.remove('.svn')  # don't visit SVN directories

    for f in files:
      for extension in extensions:
        if f.endswith(extension):
          print >> qrcfile, '<file alias="%s">%s</file>' % (os.path.join(parent, f), os.path.join(root, f))
  
  print >> qrcfile, additions
  print >> qrcfile, '</qresource>'
  print >> qrcfile, '</RCC>'
  
  qrcfile.close()		


if __name__ == '__main__':
  os.system('mkdir build')
  genqrc('build/${project.artifactId}-custom.qrc', '/skin', '${basedir}/resource/custom/${custom.skin}/skin', ['.png', '.mkv'])
  genqrc('build/${project.artifactId}-common.qrc', '/', '${basedir}/resource/common', [''])
  genqrc('build/${project.artifactId}-generated.qrc', '/', '${project.build.directory}/resource/', [''])  
  
  os.system('${environment.dir}/qt/bin/lrelease ${project.build.directory}/${project.artifactId}-specifics.pro')