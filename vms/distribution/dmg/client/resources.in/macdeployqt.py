#!/usr/bin/env python

import os
import fnmatch
import subprocess
import shutil
import sys
import plistlib
import re

from itertools import chain
from os.path import join


def change_dep_path(binary, xfrom, relative_to):
    subprocess.call(['install_name_tool', '-change', xfrom, '@executable_path/../Frameworks/' + relative_to, binary])


def remove_rpath(binary):
    output = subprocess.check_output(["otool", "-l", binary])

    # Here we parse paths from otool output which looks like this:
    #     cmd LC_RPATH
    # cmdsize 40
    #    path @executable_path/Frameworks (offset 12)
    #         ^ Path is here.           ^

    rpath_re = re.compile(
        "cmd LC_RPATH.*?cmdsize .*?path (.+?) \\(offset.*?\\)",
        re.MULTILINE | re.DOTALL)

    command = ['install_name_tool']
    for path in rpath_re.findall(output):
        command += ["-delete_rpath", path]

    command.append(binary)

    subprocess.call(command)


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

# Since Apple does not allow dots in the folders names in the bundle we have 
# to replace them with underscore and preserve folders tree by symlinks creation
def fix_qt_folder(root):
    current_directory = os.getcwd()
    root = os.path.join(current_directory, root)
    for info in os.walk(root):
        source_path = info[0]
        base_path = os.path.dirname(source_path)
        source_name = os.path.basename(source_path)
        target_name = source_name.replace('.', '_')
        if source_name == target_name:
            continue

        os.chdir(base_path)
        os.rename(source_name, target_name)
        os.symlink(target_name, source_name)
    
    os.chdir(current_directory)
        
def prepare(build_dir, binary, sbindir, tlibdir):
    tbindir = os.path.dirname(binary)
#    if os.path.exists(tbindir):
#        shutil.rmtree(tbindir)
#    os.mkdir(tbindir)

    if os.path.exists(tlibdir):
        shutil.rmtree(tlibdir)

    os.mkdir(tlibdir)

    tcontentsdir = os.path.dirname(tbindir)
    tresdir = join(tcontentsdir, 'Resources')

    applauncher_binary = join(tbindir, '@applauncher.binary.name@')
    applauncher_script = join(tbindir, '@minilauncher.binary.name@')

    shutil.copyfile(join(sbindir, '@client.binary.name@'), binary)
    shutil.copyfile(join(sbindir, '@applauncher.binary.name@'), applauncher_binary)
    shutil.copyfile(join(build_dir, 'qt.conf'), join(tbindir, 'qt.conf'))

    os.chmod(binary, 0755)
    os.chmod(applauncher_binary, 0755)
    os.chmod(applauncher_script, 0755)

    yield binary
    yield applauncher_binary

    ignore = shutil.ignore_patterns('*debug*', '.*')
    for subfolder in 'platforms', 'imageformats', 'audio', 'mediaservice':
        tfolder = join(tbindir, subfolder)
        shutil.copytree(join(sbindir, subfolder), tfolder, ignore=ignore)
        for f in os.listdir(tfolder):
            dep = join(tfolder, f)
            set_permissions(dep)
            yield dep

    tqmldir = join(tbindir, 'qml')
    shutil.copytree(join(sbindir, 'vox'), join(tresdir, 'vox'))
    shutil.copytree(join(sbindir, 'qml'), tqmldir)
    fix_qt_folder(tqmldir)

    for root, dirs, files in os.walk(tqmldir):
        for xfile in files:
            if xfile.endswith('.dylib'):
                yield join(root, xfile)

def fix_binary(binary, bindir, libdir, qlibdir, tlibdir, qtver):
    remove_rpath(binary)

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
                fix_binary(tpath, bindir, libdir, qlibdir, tlibdir, qtver)
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

                    info_plist_path = join(qlibdir, framework_name, 'Resources', 'Info.plist')
                    info_plist = open(info_plist_path).read().replace('_debug', '')
                    plist_obj = plistlib.readPlistFromString(info_plist)
                    plist_obj['CFBundleIdentifier'] = 'org.qt-project.{}'.format(name)
                    plist_obj['CFBundleVersion'] = qtver
                    plistlib.writePlist(plist_obj, join(resources_dir, 'Info.plist'))

                    os.symlink(join('Versions/Current', name), join(troot, name))
                    os.symlink('Versions/Current/Resources', join(troot, 'Resources'))
                    os.symlink('5', join(troot, 'Versions/Current'))

                    fix_binary(tpath, bindir, libdir, qlibdir, tlibdir, qtver)
                change_dep_path(binary, full_name, join(folder, name))


def main(build_dir, app_path, bindir, libdir, helpdir, qtdir, qtver):
    qlibdir = join(qtdir, 'lib')

    appdir = os.path.basename(app_path)
    app, _ = os.path.splitext(appdir)
    client_binary = "{app_path}/Contents/MacOS/{app}".format(app_path=app_path, app=app)
    tlibdir = "{app_path}/Contents/Frameworks".format(app_path=app_path)

    for binary in prepare(build_dir, client_binary, bindir, tlibdir):
        fix_binary(binary, bindir, libdir, qlibdir, tlibdir, qtver)

    resources_dir = "{app_path}/Contents/Resources".format(app_path=app_path)
    help_dir = "{}/help".format(resources_dir)
    shutil.copytree(helpdir, help_dir)
    shutil.copy(join(bindir, 'launcher.version'), resources_dir)

if __name__ == '__main__':
    _, build_dir, appdir, bindir, libdir, helpdir, qtdir, qtver = sys.argv
    main(build_dir, appdir, bindir, libdir, helpdir, qtdir, qtver)
