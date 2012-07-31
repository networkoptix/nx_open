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

def genskin():
  os.path = posixpath

  skin_qrc = open('build/skin.qrc', 'w')

  print >> skin_qrc, """
  <!DOCTYPE RCC>
  <RCC version="1.0">
  <qresource prefix="/skin">
  """

  skin_dir = '${basedir}/resource/${custom.skin}/skin'
  for root, dirs, files in os.walk(skin_dir):
    parent = root[len(skin_dir) + 1:]

    for f in files:
      if f.endswith('.png'):
        print >> skin_qrc, '<file alias="%s">%s</file>' % (os.path.join(parent, f), os.path.join(root, f))
      if f.endswith('.mkv'):
        print >> skin_qrc, '<file alias="%s">%s</file>' % (os.path.join(parent, f), os.path.join(root, f))
  
  print >> skin_qrc, '<file alias="globals.ini">${project.build.directory}/globals.ini</file>'
  print >> skin_qrc, """
  </qresource>
  </RCC>
  """  
  
  skin_qrc.close()		

def gentranslations():
  os.path = posixpath

  translations_qrc = open('build/client_translations.qrc', 'w')

  print >> translations_qrc, """
  <!DOCTYPE RCC>
  <RCC version="1.0">
  <qresource prefix="/translations">
  """

  translations_project_dir = '${project.build.sourceDirectory}/translations'
  for root, dirs, files in os.walk(translations_project_dir):
    parent = root[len(translations_project_dir) + 1:]
    if '.svn' in dirs:
      dirs.remove('.svn')  # don't visit SVN directories

    for f in files:
      if f.endswith('.qm'):
        print >> translations_qrc, '<file alias="%s">%s</file>' % (os.path.join(parent, f), os.path.join(root, f))

  print >> translations_qrc, """
  </qresource>
  </RCC>
  """  
  
  translations_qrc.close()		  

if __name__ == '__main__':
  os.system('mkdir build')
  genskin()
  os.system('${environment.dir}/qt/bin/lrelease ${project.build.directory}/${project.artifactId}-specifics.pro')
  gentranslations()
  if platform() == 'linux':
    os.system('unzip -a -u *.zip')
