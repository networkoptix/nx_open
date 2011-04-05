#!/usr/bin/env python

import shutil
import glob
from version import *
from genskin import genskin
import os, sys, posixpath
import stat, time

os.path = posixpath

FFMPEG = 'ffmpeg-git-aecd0a4'
INTRO_FILE = '../uniclient_media/intro.mov'

EXCLUDE_DIRS = ('.svn', 'dxva')
EXCLUDE_FILES = ('dxva', 'moc_', 'qrc_', 'StdAfx')

def rmtree(path):
    def on_rm_error( func, path, exc_info):
        # Dirty windows hack. Sometimes windows list already deleted files/folders.
        # Command line "rmdir /Q /S" also fails. So, just wait while the files really deleted.
        for i in xrange(20):
            if not os.listdir(path):
                break

            time.sleep(1)

        func(path)

    shutil.rmtree(path, onerror = on_rm_error)

def copy_files(src_glob, dst_folder):
    for fname in glob.iglob(src_glob):
        shutil.copy(fname, dst_folder)

def is_exclude_file(f):
    for exclude_file in EXCLUDE_FILES:
        if exclude_file in f:
            return True

    return False

def link_or_copy(src, dst):
    try:
        import win32file
        win32file.CreateHardLink(dst, src)
    except:
        shutil.copy(src, dst)

if os.path.exists('bin'):
    rmtree('bin')

if os.path.exists('build'):
    rmtree('build')

os.mkdir('bin')
os.mkdir('bin/debug')
os.mkdir('bin/release')

os.mkdir('build')

copy_files('contrib/' + FFMPEG + '/bin/debug/*.dll', 'bin/debug')
copy_files('contrib/' + FFMPEG + '/bin/release/*.dll', 'bin/release')

os.mkdir('bin/debug/arecontvision')
os.mkdir('bin/release/arecontvision')

copy_files('resource/arecontvision/*', 'bin/debug/arecontvision')
copy_files('resource/arecontvision/*', 'bin/release/arecontvision')

if os.path.exists(INTRO_FILE):
    link_or_copy(INTRO_FILE, 'bin/debug/intro.mov')
    link_or_copy(INTRO_FILE, 'bin/release/intro.mov')

version_h = open('src/version.h', 'w')
print >> version_h, '#ifndef UNIVERSAL_CLIENT_VERSION_H_'
print >> version_h, '#define UNIVERSAL_CLIENT_VERSION_H_'
print >> version_h, '// This file is generated. Go to version.py'
print >> version_h, 'static const char* ORGANIZATION_NAME="%s";' % ORGANIZATION_NAME
print >> version_h, 'static const char* APPLICATION_NAME="%s";' % APPLICATION_NAME
print >> version_h, 'static const char* APPLICATION_VERSION="%s";' % APPLICATION_VERSION
print >> version_h, '#endif // UNIVERSAL_CLIENT_VERSION_H_'

genskin()

if os.path.exists('src/uniclient.pro'):
    os.unlink('src/uniclient.pro')

shutil.copy('src/const.pro', 'src/uniclient.pro')

headers = []
sources = []

SCAN_DIR = 'src' 
for root, dirs, files in os.walk(SCAN_DIR):
    parent = root[len(SCAN_DIR)+1:]
    for exclude_dir in EXCLUDE_DIRS:
        if exclude_dir in dirs:
            dirs.remove(exclude_dir)    # don't visit SVN directories

    for f in files:
        if is_exclude_file(f):
            continue

        if f.endswith('.h'):
            headers.append(os.path.join(parent, f))
        elif f.endswith('.cpp'):
            sources.append(os.path.join(parent, f))

uniclient_pro = open('src/uniclient.pro', 'a')

for header in headers:
    print >> uniclient_pro, "HEADERS += %s" % header

for cpp in sources:
    print >> uniclient_pro,    "SOURCES += %s" % cpp

uniclient_pro.close()

if sys.platform == 'win32':
    os.system('qmake -tp vc FFMPEG=%s -o src/uniclient.vcproj src/uniclient.pro' % FFMPEG)
    os.system('src\\qmake_vc_fixer src\\uniclient.vcproj')
    os.unlink('src/uniclient.vcproj')
    os.rename('src/uniclient.new.vcproj', 'src/uniclient.vcproj')
elif sys.platform == 'darwin':
    rmtree('src/uniclient.xcodeproj')
    os.system('qmake FFMPEG=%s -o src/uniclient.xcodeproj src/uniclient.pro' % FFMPEG)
