import os, shutil

def index_common():
    oldpwd = os.getcwd()
    os.chdir('../common')
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

    for header in headers:
        print >> uniclient_pro, "HEADERS += $$PWD/%s" % header

    for cpp in sources:
        print >> uniclient_pro, "SOURCES += $$PWD/%s" % cpp

    uniclient_pro.close()
