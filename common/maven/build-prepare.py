#!/usr/bin/env python

import shutil, glob, string
import os, sys, posixpath

import time

# os.path = posixpath
sys.path.insert(0, os.path.join('..'))

from filetypes import all_filetypes, video_filetypes, image_filetypes

def gentranslations():
  os.path = posixpath

  translations_qrc = open('build/${project.artifactId}_translations.qrc', 'w')

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
        print >> translations_qrc, '<file alias="%s">%s</file>' % (os.path.join(parent, f).lower(), os.path.join(root, f).lower())

  translations_system_dir = '${environment.dir}/qt/translations'
  for root, dirs, files in os.walk(translations_system_dir):
    parent = root[len(translations_system_dir) + 1:]
    if '.svn' in dirs:
      dirs.remove('.svn')  # don't visit SVN directories

    for f in files:
      if f.endswith('.qm'):
        print >> translations_qrc, '<file alias="%s">%s</file>' % (os.path.join(parent, f).lower(), os.path.join(root, f).lower())

  print >> translations_qrc, """
  </qresource>
  </RCC>
  """  
  
  translations_qrc.close()	

def gen_filetypes_h():
    filetypes_h = open('${project.build.sourceDirectory}/plugins/resources/archive/filetypes.h', 'w')
    print >> filetypes_h, '#ifndef UNICLIENT_FILETYPES_H_'
    print >> filetypes_h, '#define UNICLIENT_FILETYPES_H_'
    print >> filetypes_h, '// This file is generated. Edit filetypes.py instead.'
    print >> filetypes_h, 'static const char* VIDEO_FILETYPES[] = {%s};' % string.join(['"' + x[0] + '"' for x in video_filetypes], ', ')
    print >> filetypes_h
    print >> filetypes_h, 'static const char* IMAGE_FILETYPES[] = {%s};' % string.join(['"' + x[0] + '"' for x in image_filetypes], ', ')
    print >> filetypes_h, '#endif // UNICLIENT_FILETYPES_H_'

if __name__ == '__main__':
  os.system('mkdir build')
  os.system('${environment.dir}/qt/bin/lrelease ${project.build.directory}/${project.artifactId}-specifics.pro')  
  gen_filetypes_h()
  gentranslations()