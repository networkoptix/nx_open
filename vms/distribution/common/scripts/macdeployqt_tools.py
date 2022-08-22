#!/usr/bin/env python3

## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import os
import io
import fnmatch
import subprocess
import shutil
import plistlib
import re

from os.path import join


def fix_rpath(binary):
    output = subprocess.check_output(["otool", "-l", binary]).decode('utf-8')

    # Here we parse paths from otool -l output which look like this:
    #     cmd LC_RPATH
    # cmdsize 40
    #    path @executable_path/Frameworks (offset 12)
    #         ^ Path is here.           ^

    rpath_re = re.compile(
        "cmd LC_RPATH.*?cmdsize .*?path (.+?) \\(offset.*?\\)",
        re.MULTILINE | re.DOTALL)

    # Delete each LC_RPATH in a separate command. This is necessary because some binaries contain
    # identical RPATHs multiple times. `install_name_tool` will fail if two or more identical
    # -delete_rpath parameters are given by CLI. Also we have to delete such RPATHs as many times
    # as they occur in the binary, because `install_name_tool` deletes only one entry per request.
    for path in rpath_re.findall(output):
        command = ["install_name_tool", "-delete_rpath", path, binary]
        subprocess.call(command)

    command = ['install_name_tool', '-add_rpath', '@executable_path/../Frameworks', binary]
    subprocess.call(command)


def binary_deps(binary):
    p = subprocess.Popen(["otool", "-L", binary], stdout=subprocess.PIPE)
    out, err = p.communicate()
    out = out.decode('utf-8')
    for line in out.splitlines():
        if line.endswith(':') or not line.strip():
            continue

        fname = line.split('(')[0].strip()
        if fname.startswith('/usr/lib/') or fname.startswith('/System/Library'):
            continue

        yield fname


def set_permissions(path):
    if os.path.isdir(path):
        os.chmod(path, 0o755)
        for root, dirs, files in os.walk(path):
            for xdir in dirs:
                os.chmod(join(root, xdir), 0o755)
            for xfile in files:
                os.chmod(join(root, xfile), 0o644)
    else:
        os.chmod(path, 0o644)


def has_adhoc_or_no_signature(fpath):
    output = subprocess.run(["codesign", "-dv", fpath], capture_output=True).stderr.decode('utf-8')
    return ("Signature=adhoc" in output) or ("code object is not signed at all" in output)


def fix_binary(binary, bindir, libdir, qlibdir, tlibdir, qtver):
    fix_rpath(binary)

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
                os.chmod(tpath, 0o644)
                # Avoid invalidating code signatures of 3rd party libraries.
                if has_adhoc_or_no_signature(fpath):
                    fix_binary(tpath, bindir, libdir, qlibdir, tlibdir, qtver)
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
                    os.chmod(tpath, 0o644)

                    resources_dir = join(tfolder, 'Resources')
                    os.mkdir(resources_dir)

                    info_plist_path = join(qlibdir, framework_name, 'Resources', 'Info.plist')
                    info_plist = open(info_plist_path, 'rb').read().replace(b'_debug', b'')

                    plist_obj = plistlib.load(io.BytesIO(info_plist))
                    if plist_obj:
                        plist_obj['CFBundleIdentifier'] = 'org.qt-project.{}'.format(name)
                        plist_obj['CFBundleVersion'] = qtver
                        plistlib.dump(plist_obj, open(join(resources_dir, 'Info.plist'), 'wb'))

                    os.symlink(join('Versions/Current', name), join(troot, name))
                    os.symlink('Versions/Current/Resources', join(troot, 'Resources'))
                    os.symlink('5', join(troot, 'Versions/Current'))

                    fix_binary(tpath, bindir, libdir, qlibdir, tlibdir, qtver)
