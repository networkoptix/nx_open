import os, sys, posixpath
sys.path.insert(0, '${basedir}/../common')
#sys.path.insert(0, os.path.join('..', '..', 'common'))
from os import listdir

from gencomp import gencomp_cpp
#os.makedirs('${project.build.directory}/build/release/generated/')
gencomp_cpp(open('${project.build.sourceDirectory}/compatibility_info.cpp', 'w'))

translations_dir='${basedir}/translations'
translations_target_dir='${project.build.directory}/resources/translations'
ldpath='${qt.dir}/lib'
translations=['${translation1}','${translation2}','${translation3}','${translation4}','${translation5}','${translation6}','${translation7}']
os.environ["DYLD_FRAMEWORK_PATH"] = '${qt.dir}/lib'
os.environ["DYLD_LIBRARY_PATH"] = '${libdir}/lib/${build.configuration}'
os.environ["LD_LIBRARY_PATH"] = '${libdir}/lib/${build.configuration}'

def platform():
    if sys.platform == 'win32':
        return 'win32'
    elif sys.platform == 'darwin':
        return 'mac'
    elif sys.platform == 'linux2':
        return 'linux'

def genqrc(qrcname, qrcprefix, path, extensions, exclusion, additions=''):
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
    	    for translation in translations:
    	        if f.endswith('_%s.ts' % translation):
        	    if '${platform}' == 'windows':
            	        os.system('${qt.dir}/bin/lrelease %s/%s -qm %s/%s.qm' % (translations_dir, f, translations_target_dir, os.path.splitext(f)[0]))
                    else:
	                os.system('export DYLD_LIBRARY_PATH=%s && export LD_LIBRARY_PATH=%s && ${qt.dir}/bin/lrelease %s/%s -qm %s/%s.qm' % (ldpath, ldpath, translations_dir, f, translations_target_dir, os.path.splitext(f)[0]))
  
    genqrc('build/${project.artifactId}-translations.qrc','/translations',    '${project.build.directory}/resources/translations', ['.qm'],'.ts')  
    genqrc('build/${project.artifactId}-custom.qrc',      '/skin',    '${basedir}/resource/custom/${custom.skin}/skin', ['.png', '.mkv', '.jpg', '.jpeg'],'.psd')
    genqrc('build/${project.artifactId}.qrc',             '/',        '${basedir}/../cpp/shared-resources/icons/${custom.skin}', [''],'.psd')
    genqrc('build/${project.artifactId}-common.qrc',      '/',        '${basedir}/resource/common', [''],'.pdb')
    genqrc('build/${project.artifactId}-generated.qrc',   '/',        '${project.build.directory}/resource', [''],'.pdb')  