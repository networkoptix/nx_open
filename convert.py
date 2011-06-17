#!/usr/bin/env python
# -*- coding: utf-8 -*-

import shutil
import glob
import string

from version import *
from genskin import genskin
import os, sys, posixpath
import stat, time
from StringIO import StringIO
from filetypes import all_filetypes, video_filetypes, image_filetypes
from string import Template

os.path = posixpath

FFMPEG_VERSION = '2011-05-24'

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

def index_dirs(xdirs, template_file, output_file, use_prefix = False):
    if os.path.exists(output_file):
        os.unlink(output_file)

    shutil.copy(template_file, output_file)

    headers = []
    sources = []

    for xdir in xdirs:
        prefix = ''

        if use_prefix:
            prefix = os.path.join('..', xdir) + '/'

        for root, dirs, files in os.walk(xdir):
            parent = root[len(xdir)+1:]
            for exclude_dir in EXCLUDE_DIRS:
                if exclude_dir in dirs:
                    dirs.remove(exclude_dir)    # don't visit SVN directories

            for f in files:
                if is_exclude_file(f):
                    continue

                if f.endswith('.h'):
                    headers.append(prefix + os.path.join(prefix, parent, f))
                elif f.endswith('.cpp'):
                    sources.append(prefix + os.path.join(prefix, parent, f))

    uniclient_pro = open(output_file, 'a')

    for header in headers:
        print >> uniclient_pro, "HEADERS += %s" % header

    for cpp in sources:
        print >> uniclient_pro,    "SOURCES += %s" % cpp

    uniclient_pro.close()

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

def gen_filetypes_h():
    filetypes_h = open('src/device_plugins/archive/filetypes.h', 'w')
    print >> filetypes_h, '#ifndef UNICLIENT_FILETYPES_H_'
    print >> filetypes_h, '#define UNICLIENT_FILETYPES_H_'
    print >> filetypes_h, '// This file is generated. Edit filetypes.py instead.'
    print >> filetypes_h, 'static const char* VIDEO_FILETYPES[] = {%s};' % string.join(['"' + x[0] + '"' for x in video_filetypes], ', ')
    print >> filetypes_h
    print >> filetypes_h, 'static const char* IMAGE_FILETYPES[] = {%s};' % string.join(['"' + x[0] + '"' for x in image_filetypes], ',')
    print >> filetypes_h, '#endif // UNICLIENT_FILETYPES_H_'

ffmpeg_path, ffmpeg_path_debug, ffmpeg_path_release = setup_ffmpeg()
openal_path = setup_openal()

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

copy_files(ffmpeg_path_debug + '/bin/*-[0-9].dll', 'bin/debug-test')
copy_files(ffmpeg_path_debug + '/bin/*-[0-9][0-9].dll', 'bin/debug-test')

copy_files(ffmpeg_path_release + '/bin/*-[0-9].dll', 'bin/release-test')
copy_files(ffmpeg_path_release + '/bin/*-[0-9][0-9].dll', 'bin/release-test')

copy_files(openal_path + '/*.dll', 'bin/release-test')
copy_files(openal_path + '/*.dll', 'bin/debug-test')
copy_files(openal_path + '/*.dll', 'bin/release')
copy_files(openal_path + '/*.dll', 'bin/debug')

os.mkdir('bin/debug/arecontvision')
os.mkdir('bin/release/arecontvision')

copy_files('resource/arecontvision/*', 'bin/debug/arecontvision')
copy_files('resource/arecontvision/*', 'bin/release/arecontvision')

if os.path.exists(INTRO_FILE):
    link_or_copy(INTRO_FILE, 'bin/debug/intro.mov')
    link_or_copy(INTRO_FILE, 'bin/release/intro.mov')

gen_version_h()
gen_filetypes_h()

genskin()


index_dirs(('src',), 'src/const.pro', 'src/uniclient.pro')
index_dirs(('src', 'test'), 'test/const.pro', 'test/uniclient_tests.pro', True)


if sys.platform == 'win32':
    os.system('qmake -tp vc FFMPEG=%s -o src/uniclient.vcproj src/uniclient.pro' % ffmpeg_path)
    os.system('qmake -tp vc FFMPEG=%s -o test/uniclient_tests.vcproj test/uniclient_tests.pro' % ffmpeg_path)
    os.system('src\\qmake_vc_fixer src\\uniclient.vcproj')
    os.unlink('src/uniclient.vcproj')
    os.rename('src/uniclient.new.vcproj', 'src/uniclient.vcproj')

    os.system('src\\qmake_vc_fixer test\\uniclient_tests.vcproj')
    os.unlink('test/uniclient_tests.vcproj')
    os.rename('test/uniclient_tests.new.vcproj', 'test/uniclient_tests.vcproj')
elif sys.platform == 'darwin':
    generate_info_plist()

    if os.path.exists('src/uniclient.xcodeproj'):
        rmtree('src/uniclient.xcodeproj')

    os.system('qmake FFMPEG=%s -o src/uniclient.xcodeproj src/uniclient.pro' % ffmpeg_path)
