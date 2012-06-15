#!/usr/bin/env python
# -*- coding: utf-8 -*-

import shutil
import glob
import string

from genskin import genskin
import os, sys, posixpath
import stat
from StringIO import StringIO
from string import Template

import re

# os.path = posixpath
sys.path.insert(0, os.path.join('..', 'common'))

from convert import index_dirs, setup_ffmpeg, gen_filetypes_h, rmtree, instantiate_pro, BUILDLIB
from convert import qt_path, copy_files, setup_tools, setup_qjson, setup_openssl, platform, gen_env_sh
from convert import convert as convert_common
from common_version import *

from filetypes import all_filetypes, video_filetypes, image_filetypes
from gencomp import gencomp_cpp

APPLICATION_NAME = 'Network Optix HD Witness Client'

EXCLUDE_DIRS = ('.svn', 'dxva')
EXCLUDE_FILES = ('dxva', 'moc_', 'qrc_', 'StdAfx')

if platform() == 'win32':
    EXCLUDE_DIRS += ()
    EXCLUDE_FILES += ('_mac',)
else:
    EXCLUDE_DIRS += ('desktop',)
    EXCLUDE_FILES += ()

def is_exclude_file(f):
    for exclude_file in EXCLUDE_FILES:
        if exclude_file in f:
            return True

    return False

def setup_openal():
    openal_path = 'contrib/openal/bin/'
    openal_path += sys.platform

    return openal_path

def generate_info_plist():
    def gen_association(ext):
        association = """
                <dict>
                    <key>CFBundleTypeExtensions</key>
                    <array>
                        <string>${ext}</string>
                    </array>
                    <key>CFBundleTypeIconFile</key>
                    <string>eve_logo.icns</string>
                    <key>CFBundleTypeName</key>
                    <string>${ext_upper} container</string>
                    <key>CFBundleTypeRole</key>
                    <string>Viewer</string>
                </dict>
        """

        association_template = Template(association)
        return association_template.substitute(ext=ext, ext_upper=ext.upper())

    xin = open('src/Info.plist.template', 'r').read()
    xout = open('src/Info.plist', 'w')

    associations = StringIO()

    for ext, guid in all_filetypes:
        print >> associations, gen_association(ext)

    strings_template = Template(xin)
    print >> xout, strings_template.substitute(associations=associations.getvalue(), version=APPLICATION_VERSION)

def gen_version_h():
    ORGANIZATION_NAME, APPLICATION_VERSION, BUILD_NUMBER, REVISION, FFMPEG_VERSION = set_env()
    print >> sys.stderr, 'Revision is %s' % REVISION
    print >> sys.stderr, 'FFMPEG version is %s' % FFMPEG_VERSION

    version_h = open('src/version.h', 'w')
    print >> version_h, '#ifndef UNIVERSAL_CLIENT_VERSION_H_'
    print >> version_h, '#define UNIVERSAL_CLIENT_VERSION_H_'
    print >> version_h, '// This file is generated. Go to version.py'
    print >> version_h, 'const char* const ORGANIZATION_NAME="%s";' % ORGANIZATION_NAME
    print >> version_h, 'const char* const APPLICATION_NAME="%s";' % APPLICATION_NAME
    print >> version_h, 'const char* const APPLICATION_VERSION="%s.%s";' % (APPLICATION_VERSION, BUILD_NUMBER)
    print >> version_h, 'const char* const APPLICATION_REVISION="%s";' % REVISION
    print >> version_h, 'const char* const FFMPEG_VERSION="%s";' % FFMPEG_VERSION
    print >> version_h, ''

    print >> version_h, '// There constans are here for windows resouce file.'

    app_version_commas = APPLICATION_VERSION.replace('.', ',')
    print >> version_h, '#define VER_FILEVERSION             %s,%s' % (app_version_commas, BUILD_NUMBER)
    print >> version_h, '#define VER_FILEVERSION_STR         "%s.%s"' % (APPLICATION_VERSION, BUILD_NUMBER)

    print >> version_h, '#define VER_PRODUCTVERSION          %s.%s' % (app_version_commas, BUILD_NUMBER)
    print >> version_h, '#define VER_PRODUCTVERSION_STR      "%s.%s"' % (APPLICATION_VERSION, BUILD_NUMBER)

    print >> version_h, '#define VER_COMPANYNAME_STR         "%s"' % ORGANIZATION_NAME
    print >> version_h, '#define VER_FILEDESCRIPTION_STR     "%s"' % APPLICATION_NAME
    print >> version_h, '#define VER_INTERNALNAME_STR        "%s"' % APPLICATION_NAME
    print >> version_h, '#define VER_LEGALCOPYRIGHT_STR      "Copyright © 2012 %s"' % ORGANIZATION_NAME
    print >> version_h, '#define VER_LEGALTRADEMARKS1_STR    "All Rights Reserved"'
    print >> version_h, '#define VER_LEGALTRADEMARKS2_STR    VER_LEGALTRADEMARKS1_STR'
    print >> version_h, '#define VER_ORIGINALFILENAME_STR    "client.exe"'
    print >> version_h, '#define VER_PRODUCTNAME_STR         "HD Witness"'

    print >> version_h, '#define VER_COMPANYDOMAIN_STR       "networkoptix.com"'
    print >> version_h, '#endif // UNIVERSAL_CLIENT_VERSION_H_'

if len(sys.argv) == 2 and sys.argv[1] == '-parents':
    convert_common()

ffmpeg_path, ffmpeg_path_debug, ffmpeg_path_release = setup_ffmpeg()
openal_path = setup_openal()
openssl_path = setup_openssl()
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

if platform() == 'mac':
    ldpath_debug = ''
    ldpath_release = ''

if platform() == 'win32':
    copy_files(ffmpeg_path_debug + '/bin/*-[0-9].dll', 'bin/debug')
    copy_files(ffmpeg_path_debug + '/bin/*-[0-9][0-9].dll', 'bin/debug')
    copy_files(ffmpeg_path_release + '/bin/*-[0-9].dll', 'bin/release')
    copy_files(ffmpeg_path_release + '/bin/*-[0-9][0-9].dll', 'bin/release')
elif platform() == 'mac':
    ldpath_debug += ffmpeg_path_debug + '/lib'
    ldpath_release += ffmpeg_path_release + '/lib'

copy_files(openal_path + '/*.dll', 'bin/release')
copy_files(openal_path + '/*.dll', 'bin/debug')

if platform() == 'win32':
    copy_files('contrib/qtcolorpicker/lib/QtSolutions_ColorPicker-2.6.dll', 'bin/release')
    copy_files('contrib/qtcolorpicker/lib/QtSolutions_ColorPicker-2.6d.dll', 'bin/debug')

if platform() == 'win32':
    copy_files(openssl_path + '/bin/*.dll', 'bin/debug')
    copy_files(openssl_path + '/bin/*.dll', 'bin/release')

    copy_files(tools_path + '/bin/*.dll', 'bin/release')
    copy_files(tools_path + '/bin/*.dll', 'bin/debug')
elif platform() == 'mac':
    ldpath_debug += ':' + tools_path + '/lib'
    ldpath_release += ':' + tools_path + '/lib'

if platform() == 'win32':
    copy_files(qjson_path + '/release/qjson.dll', 'bin/release')
    copy_files(qjson_path + '/debug/qjson.dll', 'bin/debug')
elif platform() == 'mac':
    ldpath_debug += ':' + os.path.abspath(qjson_path)
    ldpath_release += ':' + os.path.abspath(qjson_path)
elif platform() == 'linux':
    copy_files(qjson_path + '/libqjson.so.0', 'bin/release')
    copy_files(qjson_path + '/libqjson.so.0', 'bin/debug')

if platform() == 'mac':
    ldpath_debug += ':' + os.path.abspath('../common/bin/debug')
    ldpath_release += ':' + os.path.abspath('../common/bin/release')

if platform() == 'mac':
    gen_env_sh('bin/debug/env.sh', ldpath_debug, {'FFMPEG_PATH' : ffmpeg_path_debug, 'QJSON_PATH' : os.path.abspath(qjson_path)})
    gen_env_sh('bin/release/env.sh', ldpath_release, {'FFMPEG_PATH' : ffmpeg_path_release, 'QJSON_PATH' : os.path.abspath(qjson_path)})

gen_version_h()

genskin()

os.makedirs('build/generated')
gencomp_cpp(open('build/generated/compatibility_info.cpp', 'w'))

index_dirs(('src',), 'src/const.pro', 'src/client.pro', exclude_dirs=EXCLUDE_DIRS, exclude_files=EXCLUDE_FILES)
instantiate_pro('src/client.pro', {'BUILDLIB' : BUILDLIB, 'FFMPEG' : ffmpeg_path, 'EVETOOLS_DIR' : tools_path})
# index_dirs(('src', 'test'), 'test/const.pro', 'test/client_tests.pro', True, exclude_dirs=EXCLUDE_DIRS, exclude_files=EXCLUDE_FILES)

os.system('lrelease src/client.pro')

if platform() == 'win32':
    os.system('qmake -tp vc -o src/client.vcproj src/client.pro')
elif platform() == 'mac':
    generate_info_plist()

    os.system('qmake -spec macx-g++ CONFIG-=release CONFIG+=debug -o build/Makefile.debug src/client.pro')
    os.system('qmake -spec macx-g++ CONFIG-=debug CONFIG+=release -o build/Makefile.release src/client.pro')
elif platform() == 'linux':
    os.system('qmake -spec linux-g++ CONFIG-=release CONFIG+=debug -o build/Makefile.debug src/client.pro')
    os.system('qmake -spec linux-g++ CONFIG-=debug CONFIG+=release -o build/Makefile.release src/client.pro')

if platform() == 'win32':
    # Bespin. By now only for windows.
    bespin_path = 'contrib/bespin/bin/'
    ### add x86/x64 selector
    if sys.platform == 'win32':
        bespin = 'msvc-x86'
    elif sys.platform == 'darwin':
        bespin = 'mac-x86'
    else:
        bespin = 'linux-x86'
    bespin_path = os.path.join(bespin_path, bespin)

    if not os.path.isdir(bespin_path):
        print >> sys.stdout, "Can't find Bespin style plugin binaries for this platform at %s . rebuilding..." % bespin_path

        tmp_build_dir = 'build/tmp'
        if os.path.exists(tmp_build_dir):
            rmtree(tmp_build_dir)
        os.mkdir(tmp_build_dir)

        if sys.platform == 'win32':
            os.system('cd %s && qmake -r ../../contrib/bespin/bespin.pro && nmake /S && cd ../..' % tmp_build_dir)
        else:
            os.system('cd %s && qmake -spec macx-g++ CONFIG+=x86 -r ../../contrib/bespin/bespin.pro && make -j8 && cd ../..' % tmp_build_dir)

    if os.path.isdir(bespin_path):
        ### copy bespin_path/* to bin recursively ?
        os.mkdir('bin/release/styles')
        os.mkdir('bin/debug/styles')
        copy_files(bespin_path + '/release/styles/*', 'bin/release/styles')
        copy_files(bespin_path + '/debug/styles/*', 'bin/debug/styles')

    copy_files('contrib/x264/bin/x264.exe', 'bin/release/')
    copy_files('contrib/x264/bin/x264.exe', 'bin/debug/')
