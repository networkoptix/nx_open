#!/usr/bin/env python

import shutil, glob, string
import os, sys, posixpath

import time

from filetypes import all_filetypes, video_filetypes, image_filetypes

os.path = posixpath

FFMPEG_VERSION = '2011-08-29'

EXCLUDE_DIRS = ('.svn', 'dxva')
EXCLUDE_FILES = ('dxva', 'moc_', 'qrc_', 'StdAfx')

# Should be 'staticlib' or '' for DLL
BUILDLIB = 'staticlib'
# BUILDLIB = ''

def instantiate_pro(profile, params):
    class T(string.Template):
        delimiter = '%'

    pro_t = T(open(profile, 'r').read())
    open(profile, 'w').write(pro_t.substitute(params))

def rmtree(path):
    def on_rm_error(func, path, exc_info):
        # Dirty windows hack. Sometimes windows list already deleted files/folders.
        # Command line "rmdir /Q /S" also fails. So, just wait while the files really deleted.
        for i in xrange(20):
            if not os.listdir(path):
                break

            time.sleep(1)

        func(path)

    if os.path.isdir(path):
        shutil.rmtree(path, onerror = on_rm_error)
    else:
        os.unlink(path)

def gen_filetypes_h():
    filetypes_h = open('src/device_plugins/archive/filetypes.h', 'w')
    print >> filetypes_h, '#ifndef UNICLIENT_FILETYPES_H_'
    print >> filetypes_h, '#define UNICLIENT_FILETYPES_H_'
    print >> filetypes_h, '// This file is generated. Edit filetypes.py instead.'
    print >> filetypes_h, 'static const char* VIDEO_FILETYPES[] = {%s};' % string.join(['"' + x[0] + '"' for x in video_filetypes], ', ')
    print >> filetypes_h
    print >> filetypes_h, 'static const char* IMAGE_FILETYPES[] = {%s};' % string.join(['"' + x[0] + '"' for x in image_filetypes], ', ')
    print >> filetypes_h, '#endif // UNICLIENT_FILETYPES_H_'


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

def index_common():
    oldpwd = os.getcwd()
    os.chdir('../eveCommon')
    index_dirs(('src',), 'src/const.pri', 'src/common.pri')
    os.chdir(oldpwd)

def is_exclude_file(f, exclude_files):
    for exclude_file in exclude_files:
        if exclude_file in f:
            return True

    return False

def index_dirs(xdirs, template_file, output_file, use_prefix = False, exclude_dirs=(), exclude_files=()):
    if os.path.exists(output_file):
        os.unlink(output_file)

    if os.path.exists(template_file):
        shutil.copy(template_file, output_file)

    headers = []
    sources = []

    for xdir in xdirs:
        prefix = ''

        if use_prefix:
            prefix = os.path.join('..', xdir) + '/'

        for root, dirs, files in os.walk(xdir):
            parent = root[len(xdir)+1:]
            for exclude_dir in exclude_dirs:
                if exclude_dir in dirs:
                    dirs.remove(exclude_dir)    # don't visit SVN directories

            for f in files:
                if is_exclude_file(f, exclude_files):
                    continue

                if f.endswith('.h'):
                    headers.append(prefix + os.path.join(prefix, parent, f))
                elif f.endswith('.cpp'):
                    sources.append(prefix + os.path.join(prefix, parent, f))

    uniclient_pro = open(output_file, 'a')

    print >> uniclient_pro
    for header in headers:
        print >> uniclient_pro, "HEADERS += $$PWD/%s" % header

    print >> uniclient_pro
    for cpp in sources:
        print >> uniclient_pro, "SOURCES += $$PWD/%s" % cpp

    uniclient_pro.close()

if __name__ == '__main__':
    if os.path.exists('bin'):
        rmtree('bin')

    if os.path.exists('build'):
        rmtree('build')

    index_dirs(('src',), 'src/const.pro', 'src/common.pro', exclude_dirs=EXCLUDE_DIRS, exclude_files=EXCLUDE_FILES)
    instantiate_pro('src/common.pro', {'BUILDLIB': BUILDLIB})

    ffmpeg_path, ffmpeg_path_debug, ffmpeg_path_release = setup_ffmpeg()

    gen_filetypes_h()

    if sys.platform == 'win32':
        os.system('qmake -tp vc FFMPEG=%s -o src/common.vcproj src/common.pro' % ffmpeg_path)

    elif sys.platform == 'darwin':
        if os.path.exists('src/Makefile'):
            rmtree('src/Makefile')

        os.system('qmake -spec macx-g++ CONFIG-=release CONFIG+=debug FFMPEG=%s -o build/Makefile.debug src/common.pro' % ffmpeg_path)
        os.system('qmake -spec macx-g++ CONFIG-=debug CONFIG+=release FFMPEG=%s -o build/Makefile.release src/common.pro' % ffmpeg_path)
