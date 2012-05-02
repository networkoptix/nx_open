import os, posixpath

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
    if '.svn' in dirs:
      dirs.remove('.svn')  # don't visit SVN directories

    for f in files:
      if f.endswith('.png'):
        print >> skin_qrc, '<file alias="%s">%s</file>' % (os.path.join(parent, f).lower(), os.path.join(root, f).lower())
      if f.endswith('.mkv'):
        print >> skin_qrc, '<file alias="%s">%s</file>' % (os.path.join(parent, f).lower(), os.path.join(root, f).lower())
      

  print >> skin_qrc, """
  </qresource>
  </RCC>
  """

  skin_qrc.close()

if __name__ == '__main__':
  genskin()
