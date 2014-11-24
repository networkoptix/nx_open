import os, sys, posixpath, platform, fileinput, shutil, re
from os.path import dirname, join, exists, isfile
from os import listdir

versions=['1.4','1.5','2.0','2.1']
xml='contents.xml'

def get_size(start_path = '.'):
    total_size = 0
    for dirpath, dirnames, filenames in os.walk(start_path):
        for f in filenames:
            fp = os.path.join(dirpath, f)
            total_size += os.path.getsize(fp)
    return total_size

def genxml(xmlname, path):
    os.path = posixpath
    xmlfile = open(xmlname, 'w')

    print >> xmlfile, '<?xml version="1.0" encoding="UTF-8"?>'
    print >> xmlfile, '<contents totalsize="%s" validation="md5">' % get_size(path)

    for item in os.listdir(path):
        if os.path.isdir(os.path.join(path, item)):
            print >> xmlfile, '<directory>%s</directory>' % item
        else:    
            print >> xmlfile, '<file>%s</file>' % item  
    print >> xmlfile, '</contents>'
    xmlfile.close()

def genfiles(path):    
    print path  
    for root, dirs, files in os.walk(path):
        parent = root[len(path) + 1:]
        for dir in dirs:
            print os.path.join(path, parent, dir)
            if os.listdir(os.path.join(path, parent, dir)):
                if os.path.exists(os.path.join(path, xml)):
                    os.remove(os.path.join(path, xml))
                genxml(xml, os.path.join(path, parent, dir))
                if os.path.exists(os.path.join(path, parent, dir, xml)):
                    os.remove(os.path.join(path, parent, dir, xml))                
                shutil.move(os.path.join('${project.build.directory}', xml), os.path.join(path, parent, dir, xml))

if __name__ == '__main__':
    for v in versions:
        genfiles('${project.build.directory}/client-%s-${platform}-${arch}-${customization}' % v)
    for dirpath, dirnames, filenames in os.walk('.'):
    genxml(join(dirpath, xml), dirpath)