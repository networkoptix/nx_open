#!/usr/bin/env python
# -*- coding: utf-8 -*-

import shutil
import glob
import string

from version import *
import os, sys
import stat, time
from StringIO import StringIO
from string import Template

sys.path.insert(0, '../common')

from convert import index_dirs, rmtree, setup_ffmpeg, setup_qjson, instantiate_pro, copy_files, BUILDLIB, setup_tools, platform
from convert import convert as convert_common

FFMPEG_VERSION = '2011-08-29'

EXCLUDE_DIRS = ('.svn', 'dxva')
EXCLUDE_FILES = ('dxva', 'moc_', 'qrc_', 'StdAfx')

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

if len(sys.argv) == 2 and sys.argv[1] == '-parents':
    convert_common()

ffmpeg_path, ffmpeg_path_debug, ffmpeg_path_release = setup_ffmpeg()
tools_path = setup_tools()
qjson_path = setup_qjson()

if os.path.exists('bin'):
    rmtree('bin')

if os.path.exists('build'):
    rmtree('build')

os.mkdir('bin')
os.mkdir('bin/debug')
os.mkdir('bin/release')
os.mkdir('bin/debug-test')
os.mkdir('bin/release-test')

os.mkdir('build')

copy_files(ffmpeg_path_debug + '/bin/*-[0-9].dll', 'bin/debug')
copy_files(ffmpeg_path_debug + '/bin/*-[0-9][0-9].dll', 'bin/debug')

copy_files(ffmpeg_path_release + '/bin/*-[0-9].dll', 'bin/release')
copy_files(ffmpeg_path_release + '/bin/*-[0-9][0-9].dll', 'bin/release')

os.mkdir('bin/debug/arecontvision')
os.mkdir('bin/release/arecontvision')

copy_files('resource/arecontvision/*', 'bin/debug/arecontvision')
copy_files('resource/arecontvision/*', 'bin/release/arecontvision')

copy_files(tools_path + '/bin/*.dll', 'bin/release')
copy_files(tools_path + '/bin/*.dll', 'bin/debug')

if platform() == 'win32':
    copy_files(qjson_path + '/release/qjson.dll', 'bin/release')
    copy_files(qjson_path + '/debug/qjson.dll', 'bin/debug')
elif platform() == 'linux':
    copy_files(qjson_path + '/libqjson.so.0', 'bin/release')
    copy_files(qjson_path + '/libqjson.so.0', 'bin/debug')

gen_version_h()

index_dirs(('src',), 'src/const.pro', 'src/server.pro', exclude_dirs=EXCLUDE_DIRS, exclude_files=EXCLUDE_FILES)
instantiate_pro('src/server.pro', {'BUILDLIB' : BUILDLIB, 'FFMPEG' : ffmpeg_path, 'EVETOOLS_DIR' : tools_path})

if platform() == 'win32':
    os.system('qmake -tp vc -o src/server.vcproj src/server.pro')
elif platform() == 'mac':
    if os.path.exists('src/Makefile.debug'):
        os.unlink('src/Makefile.debug')

    if os.path.exists('src/Makefile.release'):
        os.unlink('src/Makefile.release')

    os.system('qmake -spec macx-g++ CONFIG-=release CONFIG+=debug -o build/Makefile.debug src/server.pro')
    os.system('qmake -spec macx-g++ CONFIG-=debug CONFIG+=release -o build/Makefile.release src/server.pro')
elif platform() == 'linux':
    if os.path.exists('src/Makefile.debug'):
        os.unlink('src/Makefile.debug')

    if os.path.exists('src/Makefile.release'):
        os.unlink('src/Makefile.release')

    os.system('qmake -spec linux-g++ CONFIG-=release CONFIG+=debug -o build/Makefile.debug src/server.pro')
    os.system('qmake -spec linux-g++ CONFIG-=debug CONFIG+=release -o build/Makefile.release src/server.pro')
