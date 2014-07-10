#!/usr/bin/env python

import shutil, glob, string, os, sys, posixpath, time

# os.path = posixpath
sys.path.insert(0, os.path.join('..'))

from filetypes import all_filetypes, video_filetypes, image_filetypes

def gen_filetypes_h():
    filetypes_h = open('${project.build.sourceDirectory}/plugins/resource/avi/filetypes.h', 'w')
    print >> filetypes_h, '#ifndef UNICLIENT_FILETYPES_H_'
    print >> filetypes_h, '#define UNICLIENT_FILETYPES_H_'
    print >> filetypes_h, '// This file is generated. Edit filetypes.py instead.'
    print >> filetypes_h, 'static const char* VIDEO_FILETYPES[] = {%s};' % string.join(['"' + x[0] + '"' for x in video_filetypes], ', ')
    print >> filetypes_h
    print >> filetypes_h, 'static const char* IMAGE_FILETYPES[] = {%s};' % string.join(['"' + x[0] + '"' for x in image_filetypes], ', ')
    print >> filetypes_h, '#endif // UNICLIENT_FILETYPES_H_'

if __name__ == '__main__':
    gen_filetypes_h()
