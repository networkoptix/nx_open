#!/usr/bin/env python3

## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import os
import shutil
import sys

from os.path import join
from macdeployqt_tools import set_permissions, fix_binary


def prepare(qt_dir, build_dir, binary, sbindir, tlibdir):
    tbindir = os.path.dirname(binary)

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

    os.chmod(binary, 0o755)
    os.chmod(applauncher_binary, 0o755)
    os.chmod(applauncher_script, 0o755)

    yield binary
    yield applauncher_binary

    ignore = shutil.ignore_patterns('*debug*', '.*')
    src_plugins_dir = join(qt_dir, "plugins")
    plugins_dir = join(tcontentsdir, "PlugIns")
    for group in 'platforms', 'imageformats', 'audio', 'mediaservice':
        target_dir = join(plugins_dir, group)
        shutil.copytree(join(src_plugins_dir, group), target_dir, ignore=ignore)
        for f in os.listdir(target_dir):
            dep = join(target_dir, f)
            set_permissions(dep)
            yield dep

    qml_dir = join(tresdir, 'qml')
    shutil.copytree(join(qt_dir, 'qml'), qml_dir)
    for root, _, files in os.walk(qml_dir):
        for f in files:
            if f.endswith('.dylib'):
                yield join(root, f)


def main(build_dir, app_path, bindir, libdir, helpdir, qtdir, qtver):
    qlibdir = join(qtdir, 'lib')

    appdir = os.path.basename(app_path)
    app, _ = os.path.splitext(appdir)
    client_binary = "{app_path}/Contents/MacOS/{app}".format(app_path=app_path, app=app)
    tlibdir = "{app_path}/Contents/Frameworks".format(app_path=app_path)

    for binary in prepare(qtdir, build_dir, client_binary, bindir, tlibdir):
        fix_binary(binary, bindir, libdir, qlibdir, tlibdir, qtver)

    resources_dir = "{app_path}/Contents/Resources".format(app_path=app_path)
    help_dir = "{}/help".format(resources_dir)
    shutil.copytree(helpdir, help_dir)
    shutil.copy(join(bindir, 'launcher.version'), resources_dir)

if __name__ == '__main__':
    _, build_dir, appdir, bindir, libdir, helpdir, qtdir, qtver = sys.argv
    main(build_dir, appdir, bindir, libdir, helpdir, qtdir, qtver)
