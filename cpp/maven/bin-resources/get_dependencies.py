#!/usr/bin/env python
# -*- coding: utf-8 -*-

import os, sys, shutil
import dependencies

sys.path.insert(0, dependencies.RDEP_PATH)
import rdep
sys.path.pop(0)

debug = "debug" in dependencies.BUILD_CONFIGURATION

def fetch_packages(packages):
    return rdep.fetch_packages(dependencies.TARGET, packages, debug)

def copy_recursive(src, dst):
    for dirname, _, filenames in os.walk(src):
        rel_dir = os.path.relpath(dirname, src)
        dir = os.path.abspath(os.path.join(dst, rel_dir))
        if not os.path.isdir(dir):
            os.makedirs(dir)
        for filename in filenames:
            entry = os.path.join(dirname, filename)
            shutil.copy2(entry, dir)

def install_dependency(dependency_dir, target_dir, debug):
    bin_dir = os.path.join(dependency_dir, "bin")
    bin_dst = os.path.join(target_dir, "bin")

    if os.path.exists(bin_dir):
        debug_dir = os.path.join(bin_dst, "debug")
        if not os.path.isdir(debug_dir):
            os.path.mkdirs(debug_dir)
        copy_recursive(bin_dir, debug_dir)

        if not debug:
            release_dir = os.path.join(bin_dst, "release")
            if not os.path.isdir(release_dir):
                os.path.mkdirs(release_dir)
            copy_recursive(bin_dir, release_dir)

def copy_package_for_configuration(package, path, debug):
    target_dir = dependencies.TARGET_DIRECTORY

    description_file = os.path.join(target_dir, package + rdep.PACKAGE_CONFIG_NAME)

    if os.path.isfile(description_file):
        return True

    print "Copying package {0} into {1}".format(package, target_dir)
    install_dependency(path, target_dir, debug)

    shutil.copy(os.path.join(path, rdep.PACKAGE_CONFIG_NAME), description_file)
    return True

def copy_package(package):
# TODO:
# 1) read copy-target list from artifact, by default - bin subdirectory
# 2) copy only selected resources to the target dir(s?), e.g. TARGET_DIRECTORY\x64\bin\debug
# QT scenario is located in the copy_additional_resources.py script. After _all_ is implemented, is should be removed.
    release_path = rdep.locate_package(dependencies.TARGET, package)
    if not release_path:
        print "Could not locate {0}".format(package)
        return False

    copy_package_for_configuration(package, release_path, False)

    if debug:
        debug_path = rdep.locate_package(dependencies.TARGET, package, debug)
        if not debug_path:
            print "Could not locate {0}".format(package)
            return False

        if debug_path == release_path:
            return True

        copy_package_for_configuration(package, debug_path, True)

    return True

def copy_packages(packages):
    return all([copy_package(package) for package in packages])


def get_dependencies():
    packages = dependencies.get_packages()
    if not packages:
        print "No dependencies found"
        return

    dependencies.print_configuration()

    if not fetch_packages(packages):
        print "Cannot fetch packages"
        sys.exit(1)

    if not copy_packages(packages):
        print "Cannot copy packages"
        sys.exit(1)


if __name__ == '__main__':
    get_dependencies()
