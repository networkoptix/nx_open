#!/usr/bin/env python

import os
import fnmatch
import subprocess
import shutil
import sys

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


def prepare(binary, sbindir, tlibdir):
    tbindir = os.path.dirname(binary)
    if os.path.exists(tbindir):
        shutil.rmtree(tbindir)
    os.mkdir(tbindir)

    if os.path.exists(tlibdir):
        shutil.rmtree(tlibdir)

    os.mkdir(tlibdir)

    shutil.copyfile(join(sbindir, 'client'), binary)
    os.chmod(binary, 0755)
    yield binary

    ignore_debug = shutil.ignore_patterns('*debug*')
    for subfolder in 'platforms', 'styles', 'imageformats':
        tfolder = join(tbindir, subfolder)
        shutil.copytree(join(sbindir, subfolder), tfolder, ignore=ignore_debug)
        for f in os.listdir(tfolder):
            dep = join(tfolder, f)
            os.chmod(dep, 0644)
            yield dep

    shutil.copytree(join(sbindir, 'vox'), join(tbindir, 'vox'))


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
            framework_name = '{name}.framework'.format(name=name)
            if framework_name in qframeworks:
                folder = path[path.find(framework_name):]
                if not os.path.exists(join(tlibdir, folder)):
                    os.makedirs(join(tlibdir, folder))

                if not os.path.exists(join(tlibdir, folder, name)):
                    fpath = join(qlibdir, folder, name)
                    tfolder = join(tlibdir, folder)
                    tpath = join(tfolder, name)

                    # Copy framework binary
                    shutil.copy(fpath, tfolder)
                    os.chmod(tpath, 0644)
                    shutil.copytree(join(qlibdir, framework_name, 'Contents'), join(tlibdir, framework_name, 'Contents'))
                    fix_binary(tpath, bindir, libdir, qlibdir, tlibdir)
                change_dep_path(binary, full_name, join(folder, name))


def main(app_path, bindir, libdir, helpdir):
    qlibdir = libdir

    appdir = os.path.basename(app_path)
    app, _ = os.path.splitext(appdir)
    client_binary = "{app_path}/Contents/MacOS/{app}".format(app_path=app_path, app=app)
    tlibdir = "{app_path}/Contents/Frameworks".format(app_path=app_path)

    for binary in prepare(client_binary, bindir, tlibdir):
        fix_binary(binary, bindir, libdir, qlibdir, tlibdir)

    shutil.copytree(helpdir, "{app_path}/Contents/help".format(app_path=app_path))

if __name__ == '__main__':
    _, appdir, bindir, libdir, helpdir = sys.argv
    main(appdir, bindir, libdir, helpdir)
