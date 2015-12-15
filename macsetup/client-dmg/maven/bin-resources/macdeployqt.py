#!/usr/bin/env python

import os
import fnmatch
import subprocess
import shutil
import sys
import plistlib

from os.path import join


def change_dep_path(binary, xfrom, relative_to):
    subprocess.call(['install_name_tool', '-change', xfrom, '@executable_path/../Frameworks/' + relative_to, binary])


def binary_deps(binary):
    p = subprocess.Popen(["otool", "-L", binary], stdout=subprocess.PIPE)
    out, err = p.communicate()
    for line in out.splitlines():
        if line.endswith(':') or not line.strip():
            continue

        fname = line.split('(')[0].strip()
        if fname.startswith('/usr/lib/') or fname.startswith('/System/Library'):
            continue

        yield fname


def set_permissions(path):
    if os.path.isdir(path):
        os.chmod(path, 0755)
        for root, dirs, files in os.walk(path):
            for xdir in dirs:
                os.chmod(join(root, xdir), 0755)
            for xfile in files:
                os.chmod(join(root, xfile), 0644)
    else:
        os.chmod(path, 0644)

	
def prepare(binary, sbindir, tlibdir):
    tbindir = os.path.dirname(binary)
    if os.path.exists(tbindir):
        shutil.rmtree(tbindir)
    os.mkdir(tbindir)

    if os.path.exists(tlibdir):
        shutil.rmtree(tlibdir)

    os.mkdir(tlibdir)

    tresdir = join(os.path.dirname(tbindir), 'Resources')

    shutil.copyfile(join(sbindir, 'client.bin'), binary)
    os.chmod(binary, 0755)
    yield binary

    ignore = shutil.ignore_patterns('*debug*', '.*')
    for subfolder in 'platforms', 'styles', 'imageformats':
        tfolder = join(tbindir, subfolder)
        shutil.copytree(join(sbindir, subfolder), tfolder, ignore=ignore)
        for f in os.listdir(tfolder):
            dep = join(tfolder, f)
            set_permissions(dep)
            yield dep

    shutil.copytree(join(sbindir, 'vox'), join(tresdir, 'vox'))


def fix_binary(binary, bindir, libdir, qlibdir, tlibdir):
    libs = fnmatch.filter(os.listdir(libdir), 'lib*dylib*')
    qframeworks = fnmatch.filter(os.listdir(qlibdir), '*.framework')

    dependencies = binary_deps(binary)
    for full_name in dependencies:
        path, name = os.path.split(full_name)
        if name.startswith('lib'):
            fpath = join(libdir, name) if name in libs else full_name
            if fpath.startswith('@'):
                return

            if not os.path.isfile(fpath):
                continue

            tpath = join(tlibdir, name)
            if not os.path.exists(tpath):
                shutil.copy(fpath, tlibdir)
                os.chmod(tpath, 0644)
                fix_binary(tpath, bindir, libdir, qlibdir, tlibdir)
            change_dep_path(binary, full_name, name)
        elif name.startswith('Q'):
            # name: QtCore
            #
            # QtCore.framework
            framework_name = '{name}.framework'.format(name=name)
            if framework_name in qframeworks:
                # QtCore.framework/Versions/5
                folder = path[path.find(framework_name):]
                if not os.path.exists(join(tlibdir, folder)):
                    os.makedirs(join(tlibdir, folder))

                if not os.path.exists(join(tlibdir, folder, name)):
                    # <source>/QtCore.framework/Versions/5/QtCore
                    fpath = join(qlibdir, folder, name)

                    # <target>/QtCore.framework
                    troot = join(tlibdir, framework_name)

                    # <target>/QtCore.framework/Versions/5
                    tfolder = join(tlibdir, folder)

                    # <target>/QtCore.framework/Versions/5/QtCore
                    tpath = join(tfolder, name)

                    # Copy framework binary
                    shutil.copy(fpath, tfolder)
                    os.chmod(tpath, 0644)

                    resources_dir = join(tfolder, 'Resources')
                    os.mkdir(resources_dir)

                    info_plist_path = join(qlibdir, framework_name, 'Contents', 'Info.plist')
                    info_plist = open(info_plist_path).read().replace('_debug', '')
                    plist_obj = plistlib.readPlistFromString(info_plist)
                    plist_obj['CFBundleIdentifier'] = 'org.qt-project.{}'.format(name)
                    plist_obj['CFBundleVersion'] = '5.2.1'
                    plistlib.writePlist(plist_obj, join(resources_dir, 'Info.plist'))

                    os.symlink(join('Versions/Current', name), join(troot, name))
                    os.symlink('Versions/Current/Resources', join(troot, 'Resources'))
                    os.symlink('5', join(troot, 'Versions/Current'))

                    fix_binary(tpath, bindir, libdir, qlibdir, tlibdir)
                change_dep_path(binary, full_name, join(folder, name))


def main(app_path, bindir, libdir, helpdir):
    qlibdir = os.path.dirname(libdir)

    appdir = os.path.basename(app_path)
    app, _ = os.path.splitext(appdir)
    client_binary = "{app_path}/Contents/MacOS/{app}".format(app_path=app_path, app=app)
    tlibdir = "{app_path}/Contents/Frameworks".format(app_path=app_path)

    for binary in prepare(client_binary, bindir, tlibdir):
        fix_binary(binary, bindir, libdir, qlibdir, tlibdir)

    shutil.copytree(helpdir, "{app_path}/Contents/Resources/help".format(app_path=app_path))

if __name__ == '__main__':
    _, appdir, bindir, libdir, helpdir = sys.argv
    main(appdir, bindir, libdir, helpdir)
