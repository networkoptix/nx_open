#!/usr/bin/env python
#!/usr/bin/env python

import shutil, glob, string, array
import os, sys, posixpath
from platform import architecture

import time

from filetypes import all_filetypes, video_filetypes, image_filetypes

# os.path = posixpath

FFMPEG_VERSION = '2012-04-10'

EXCLUDE_DIRS = ('.svn', 'dxva')
EXCLUDE_FILES = ('dxva', 'moc_', 'qrc_', 'StdAfx')

# Should be 'staticlib' or '' for DLL
BUILDLIB = 'staticlib'
# BUILDLIB = ''

# Temporary hack for development. There are some problems on mac with debugging static libs.
if sys.platform == 'darwin':
    BUILDLIB = ''

def platform():
    if sys.platform == 'win32':
        return 'win32'
    elif sys.platform == 'darwin':
        return 'mac'
    elif sys.platform == 'linux2':
        return 'linux'

def gen_env_sh(path, ldpath, env):
    if platform() == 'mac':
        ldvar = 'DYLD_LIBRARY_PATH'
    elif platform() == 'linux':
        ldvar = 'LD_LIBRARY_PATH'

    f = open(path, 'w')
    print >> f, 'export %s=%s' % (ldvar, ldpath)

    for key, value in env.items():
        print >> f, 'export %s=%s' % (key, value)

    f.close()

def link_or_copy(src, dst):
    try:
        import win32file
        win32file.CreateHardLink(dst, src)
    except:
        shutil.copy(src, dst)

def copy_files(src_glob, dst_folder):
    for fname in glob.iglob(src_glob):
        shutil.copy(fname, dst_folder)


def qt_path(path):
    if not path:
        return path

    qtpath = path.replace('\\', '/')
    if platform() == 'win32':
        qtpath = qtpath.lstrip('/')
    return qtpath

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

        if os.listdir(path):
            print >> sys.stderr, "Couldn't remove", path
            sys.exit(1)

        func(path)

    if os.path.isdir(path):
        shutil.rmtree(path, onerror = on_rm_error)
    else:
        os.unlink(path)

def gen_filetypes_h():
    filetypes_h = open('src/plugins/resources/archive/filetypes.h', 'w')
    print >> filetypes_h, '#ifndef UNICLIENT_FILETYPES_H_'
    print >> filetypes_h, '#define UNICLIENT_FILETYPES_H_'
    print >> filetypes_h, '// This file is generated. Edit filetypes.py instead.'
    print >> filetypes_h, 'static const char* VIDEO_FILETYPES[] = {%s};' % string.join(['"' + x[0] + '"' for x in video_filetypes], ', ')
    print >> filetypes_h
    print >> filetypes_h, 'static const char* IMAGE_FILETYPES[] = {%s};' % string.join(['"' + x[0] + '"' for x in image_filetypes], ', ')
    print >> filetypes_h, '#endif // UNICLIENT_FILETYPES_H_'

def host_platform_bits():
    return architecture()[0][0:2]

def setup_openssl():
    openssl_path = '../common/contrib/openssl'

    return openssl_path

def setup_tools():
    os_bit = ''
    if platform() == 'linux' and array.array('L').itemsize != 4:
        os_bit = '64'

    tools_path = os.getenv('EVE_TOOLS')

    if not tools_path or not os.path.isdir(tools_path):
        tools_path = os.path.join(os.path.dirname(__file__), '..', '..', 'evetools')

    tools_path = os.path.abspath(tools_path)
    if not os.path.isdir(tools_path):
        print 'Please clone ssh://hg@noptix.dyndns.biz/evetools to ../.. or to directory referenced by EVE_TOOLS env variable'
#        sys.exit(1)
#        return qt_path(os.path.join(os.path.dirname(__file__), 'setup', 'build'))
    return qt_path(os.path.join(tools_path, platform() + os_bit))


def setup_ffmpeg():
    ffmpeg_path = os.getenv('EVE_FFMPEG')

    if not ffmpeg_path or not os.path.isdir(ffmpeg_path):
        ffmpeg_path = os.path.join('..', '..', 'ffmpeg')

    ffmpeg_path = os.path.abspath(ffmpeg_path)
    if not os.path.isdir(ffmpeg_path):
        print 'Please clone ssh://hg@noptix.dyndns.biz/ffmpeg to ../.. or to directory referenced by EVE_FFMPEG env variable'
        sys.exit(1)

    ffmpeg = 'ffmpeg-git-' + FFMPEG_VERSION
    if platform() == 'win32':
        ffmpeg += '-mingw'
    elif platform() == 'mac':
        ffmpeg += '-mac'
    elif platform() == 'linux':
        ffmpeg += '-linux' + '-' + host_platform_bits()


    ffmpeg_path = qt_path(os.path.join(ffmpeg_path, ffmpeg))
    ffmpeg_path_debug = ffmpeg_path + '-debug'
    ffmpeg_path_release = ffmpeg_path + '-release'

    if not os.path.isdir(ffmpeg_path_debug):
        print >> sys.stderr, "Can't find directory %s. Make sure variable EVE_FFMPEG is correct." % ffmpeg_path_debug
        sys.exit(1)

    if not os.path.isdir(ffmpeg_path_release):
        print >> sys.stderr, "Can't find directory %s. Make sure variable EVE_FFMPEG is correct." % ffmpeg_path_release
        sys.exit(1)

    return ffmpeg_path, ffmpeg_path_debug, ffmpeg_path_release

def is_exclude_file(f, exclude_files):
    for exclude_file in exclude_files:
        if exclude_file in f:
            return True

    return False

def index_dirs(xdirs, template_file, output_file, exclude_dirs=(), exclude_files=()):
    if os.path.exists(output_file):
        os.unlink(output_file)

    if os.path.exists(template_file):
        shutil.copy(template_file, output_file)

    headers = []
    sources = []

    for xdir in xdirs:
        for root, dirs, files in os.walk(xdir):
            parent = root[len(xdir)+1:]
            for exclude_dir in exclude_dirs:
                if exclude_dir in dirs:
                    dirs.remove(exclude_dir)

            for f in files:
                if is_exclude_file(f, exclude_files):
                    continue

                if f.endswith('.h'):
                    headers.append(os.path.join(parent, f))
                elif f.endswith('.cpp'):
                    sources.append(os.path.join(parent, f))
                elif f.endswith('.c'):
                    sources.append(os.path.join(parent, f))

    uniclient_pro = open(output_file, 'a')

    print >> uniclient_pro
    for header in headers:
        print >> uniclient_pro, "HEADERS += $$PWD/%s" % qt_path(header)

    print >> uniclient_pro
    for cpp in sources:
        print >> uniclient_pro, "SOURCES += $$PWD/%s" % qt_path(cpp)

    uniclient_pro.close()

def convert():
    oldpwd = os.getcwd()
    os.chdir(os.path.dirname(os.path.realpath(__file__)))

    try:
        if os.path.exists('bin'):
            rmtree('bin')

        if os.path.exists('build'):
            rmtree('build')
        os.mkdir('build')

        ffmpeg_path, ffmpeg_path_debug, ffmpeg_path_release = setup_ffmpeg()
        gen_filetypes_h()
        tools_path = setup_tools()
        
        index_dirs(('src',), 'src/const.pro', 'src/common.pro', exclude_dirs=EXCLUDE_DIRS, exclude_files=EXCLUDE_FILES)
        instantiate_pro('src/common.pro', {'BUILDLIB': BUILDLIB, 'FFMPEG' : ffmpeg_path, 'EVETOOLS_DIR' : tools_path})

        if platform() == 'win32':
            os.system('qmake -tp vc -o src/common.vcproj src/common.pro')

        elif platform() == 'mac':
            os.system('qmake -spec macx-g++ CONFIG-=release CONFIG+=debug -o build/Makefile.debug src/common.pro')
            os.system('qmake -spec macx-g++ CONFIG-=debug CONFIG+=release -o build/Makefile.release src/common.pro')
        elif platform() == 'linux':
            os.system('qmake -spec linux-g++ CONFIG-=release CONFIG+=debug -o build/Makefile.debug src/common.pro')
            os.system('qmake -spec linux-g++ CONFIG-=debug CONFIG+=release -o build/Makefile.release src/common.pro')
    finally:
        os.chdir(oldpwd)
    

if __name__ == '__main__':
    convert()
