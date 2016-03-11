import os, sys, posixpath, platform, subprocess, fileinput, shutil, re
from subprocess import Popen, PIPE
from os.path import dirname, join, exists, isfile
from os import listdir

sys.path.insert(0, '${CMAKE_SOURCE_DIR}/common')
#from gencomp import gencomp_cpp

def fileIsAllowed(file, exclusions):
    for exclusion in exclusions:
        if file.endswith(exclusion):
            return False
    return True
        
def genqrc(qrcname, qrcprefix, pathes, exclusions, additions=''):
    os.path = posixpath

    qrcfile = open(qrcname, 'w')

    print >> qrcfile, '<!DOCTYPE RCC>'
    print >> qrcfile, '<RCC version="1.0">'
    print >> qrcfile, '<qresource prefix="%s">' % (qrcprefix)

    for path in pathes:
        for root, dirs, files in os.walk(path):
            parent = root[len(path) + 1:]
            for f in files:
                if fileIsAllowed(f, exclusions):
                    print >> qrcfile, '<file alias="%s">%s</file>' % (os.path.join(parent, f), os.path.join(root, f))
                    print "Generating resource: %s" % os.path.join(root, f)
    print >> qrcfile, additions
    print >> qrcfile, '</qresource>'
    print >> qrcfile, '</RCC>'
  
    qrcfile.close()
                    
if __name__ == '__main__':
#    gencomp_cpp(open('c:\\develop\\cmake\\!\\nxtool\\src/compatibility_info.cpp', 'w'))
    exceptions = ['vmsclient.png', '.ai', '.svg', '.profile']
    genqrc('${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_SHORTNAME}.qrc', '/', ['${CMAKE_CURRENT_BINARY_DIR}/resources', '${CMAKE_CURRENT_SOURCE_DIR}/resources/static'], exceptions)
