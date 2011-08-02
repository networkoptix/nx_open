#!/usr/bin/env python
# -*- coding: utf-8 -*-

import shutil
import glob
import string

from version import *
import os, sys, posixpath
import stat, time
from StringIO import StringIO
from string import Template

os.path = posixpath
sys.path.insert(0, '../common')

from convert import index_dirs, index_common

FFMPEG_VERSION = '2011-05-24'

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

def link_or_copy(src, dst):
    try:
        import win32file
        win32file.CreateHardLink(dst, src)
    except:
        shutil.copy(src, dst)


def setup_ffmpeg():
    ffmpeg_path = os.getenv('EVE_FFMPEG').replace('\\', '/')

    if not ffmpeg_path:
        print r"""EVE_FFMPEG environment variable is not defined.
        
    Do the following:
    1. Clone repository ssh://hg@vigasin.com/ffmpeg to somewhere, say c:\programming\ffmpeg
    2. Go to c:\programming\ffmpeg and run get_ffmpegs.bat
    3. Add system environment variable EVE_FFMPEG with value c:\programming\ffmpeg
    """
        sys.exit(1)

    ffmpeg = 'ffmpeg-git-' + FFMPEG_VERSION
    if sys.platform == 'win32':
        ffmpeg += '-mingw'
    else:
        ffmpeg += '-macos'


    ffmpeg_path = os.path.join(ffmpeg_path, ffmpeg)
    ffmpeg_path_debug = ffmpeg_path + '-debug'
    ffmpeg_path_release = ffmpeg_path + '-release'

    if not os.path.isdir(ffmpeg_path_debug):
        print >> sys.stderr, "Can't find directory %s. Make sure variable EVE_FFMPEG is correct." % ffmpeg_path_debug
        sys.exit(1)

    if not os.path.isdir(ffmpeg_path_release):
        print >> sys.stderr, "Can't find directory %s. Make sure variable EVE_FFMPEG is correct." % ffmpeg_path_release
        sys.exit(1)

    return ffmpeg_path, ffmpeg_path_debug, ffmpeg_path_release

def setup_openal():
    openal_path = 'contrib/openal/bin/'
    openal_path += sys.platform

    return openal_path

def gen_version_h():
    version_h = open('src/version.h', 'w')
    print >> version_h, '#ifndef UNIVERSAL_CLIENT_VERSION_H_'
    print >> version_h, '#define UNIVERSAL_CLIENT_VERSION_H_'
    print >> version_h, '// This file is generated. Go to version.py'
    print >> version_h, 'static const char* ORGANIZATION_NAME="%s";' % ORGANIZATION_NAME
    print >> version_h, 'static const char* APPLICATION_NAME="%s";' % APPLICATION_NAME
    print >> version_h, 'static const char* APPLICATION_VERSION="%s";' % APPLICATION_VERSION
    print >> version_h, ''

    print >> version_h, '// There constans are here for windows resouce file.'

    app_version_commas = APPLICATION_VERSION.replace('.', ',')
    print >> version_h, '#define VER_FILEVERSION             %s,0' % app_version_commas
    print >> version_h, '#define VER_FILEVERSION_STR         "%s.0\0"' % APPLICATION_VERSION

    print >> version_h, '#define VER_PRODUCTVERSION          %s' % app_version_commas
    print >> version_h, '#define VER_PRODUCTVERSION_STR      "%s\0"' % APPLICATION_VERSION

    print >> version_h, '#define VER_COMPANYNAME_STR         "%s"' % ORGANIZATION_NAME
    print >> version_h, '#define VER_FILEDESCRIPTION_STR     "%s"' % APPLICATION_NAME
    print >> version_h, '#define VER_INTERNALNAME_STR        "%s"' % APPLICATION_NAME
    print >> version_h, '#define VER_LEGALCOPYRIGHT_STR      "Copyright © 2011 %s"' % ORGANIZATION_NAME
    print >> version_h, '#define VER_LEGALTRADEMARKS1_STR    "All Rights Reserved"'
    print >> version_h, '#define VER_LEGALTRADEMARKS2_STR    VER_LEGALTRADEMARKS1_STR'
    print >> version_h, '#define VER_ORIGINALFILENAME_STR    "EvePlayer-Beta.exe"'
    print >> version_h, '#define VER_PRODUCTNAME_STR         "EvePlayer"'

    print >> version_h, '#define VER_COMPANYDOMAIN_STR       "networkoptix.com"'
    print >> version_h, '#endif // UNIVERSAL_CLIENT_VERSION_H_'

ffmpeg_path, ffmpeg_path_debug, ffmpeg_path_release = setup_ffmpeg()
openal_path = setup_openal()

if os.path.exists('bin'):
    rmtree('bin')

if os.path.exists('build'):
    rmtree('build')

os.mkdir('bin')
os.mkdir('bin/debug')
os.mkdir('bin/release')

os.mkdir('build')

copy_files(ffmpeg_path_debug + '/bin/*-[0-9].dll', 'bin/debug')
copy_files(ffmpeg_path_debug + '/bin/*-[0-9][0-9].dll', 'bin/debug')

copy_files(ffmpeg_path_release + '/bin/*-[0-9].dll', 'bin/release')
copy_files(ffmpeg_path_release + '/bin/*-[0-9][0-9].dll', 'bin/release')

copy_files(openal_path + '/*.dll', 'bin/release')
copy_files(openal_path + '/*.dll', 'bin/debug')

os.mkdir('bin/debug/arecontvision')
os.mkdir('bin/release/arecontvision')

copy_files('resource/arecontvision/*', 'bin/debug/arecontvision')
copy_files('resource/arecontvision/*', 'bin/release/arecontvision')

gen_version_h()


index_common()

index_dirs(('src',), 'src/const.pro', 'src/uniclient.pro', exclude_dirs=EXCLUDE_DIRS, exclude_files=EXCLUDE_FILES)


if sys.platform == 'win32':
    os.system('qmake -tp vc FFMPEG=%s -o src/uniclient.vcproj src/uniclient.pro' % ffmpeg_path)

elif sys.platform == 'darwin':
    if os.path.exists('src/uniclient.xcodeproj'):
        rmtree('src/uniclient.xcodeproj')

    os.system('qmake FFMPEG=%s -o src/uniclient.xcodeproj src/uniclient.pro' % ffmpeg_path)
