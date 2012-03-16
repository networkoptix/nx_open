#!/usr/bin/env python
# -*- coding: utf-8 -*-

import shutil
import glob
import string

from version import *
import os, sys, posixpath
import stat
from StringIO import StringIO
from string import Template

import re

# os.path = posixpath
sys.path.insert(0, os.path.join('..', 'common'))

from convert import index_dirs, rmtree, instantiate_pro, copy_files, BUILDLIB, platform
from convert import gen_env_sh

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

if platform() == 'mac':
    ldpath_debug = ''
    ldpath_release = ''


if platform() == 'mac':
    gen_env_sh('bin/debug/env.sh', ldpath_debug)
    gen_env_sh('bin/release/env.sh', ldpath_release)

gen_version_h()

index_dirs(('src',), 'src/const.pro', 'src/testCamera.pro', exclude_dirs=EXCLUDE_DIRS, exclude_files=EXCLUDE_FILES)
instantiate_pro('src/testCamera.pro', {'BUILDLIB' : BUILDLIB})

if platform() == 'win32':
    os.system('qmake -tp vc -o src/testCamera.vcproj src/testCamera.pro')
elif platform() == 'mac':
    if os.path.exists('src/Makefile.debug'):
        os.unlink('src/Makefile.debug')

    if os.path.exists('src/Makefile.release'):
        os.unlink('src/Makefile.release')

    os.system('qmake -spec macx-g++ CONFIG-=release CONFIG+=debug -o build/Makefile.debug src/testCamera.pro')
    os.system('qmake -spec macx-g++ CONFIG-=debug CONFIG+=release -o build/Makefile.release src/testCamera.pro')
elif platform() == 'linux':
    if os.path.exists('src/Makefile.debug'):
        os.unlink('src/Makefile.debug')

    if os.path.exists('src/Makefile.release'):
        os.unlink('src/Makefile.release')

    os.system('qmake -spec linux-g++ CONFIG-=release CONFIG+=debug -o build/Makefile.debug src/testCamera.pro')
    os.system('qmake -spec linux-g++ CONFIG-=debug CONFIG+=release -o build/Makefile.release src/testCamera.pro')
