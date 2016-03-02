#!/usr/bin/env python
# -*- coding: utf-8 -*-

import os, sys, shutil
import dependencies
import ConfigParser

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

def get_copy_list(package_dir):
    config = ConfigParser.ConfigParser()
    config.read(os.path.join(package_dir, rdep.PACKAGE_CONFIG_NAME))

    if not config.has_option("General", "copy"):
        return [ "bin" ]

    return config.get("General", "copy").split()

def install_dependency(dependency_dir, target_dir, debug):
    print "Installing dependency from {0}".format(dependency_dir)
    bin_dst = os.path.join(target_dir, "bin")

    copy_list = get_copy_list(dependency_dir)

    for entry in copy_list:
        src = os.path.join(dependency_dir, entry)

        if not os.path.exists(src):
            continue

        debug_dst = os.path.join(bin_dst, "debug", os.path.basename(entry))
        if not os.path.isdir(debug_dst):
            os.makedirs(debug_dst)
        print "Copying {0} to {1}".format(src, debug_dst)
        copy_recursive(src, debug_dst)

        if not debug:
            release_dst = os.path.join(bin_dst, "release", os.path.basename(entry))
            if not os.path.isdir(release_dst):
                os.makedirs(release_dst)
            print "Copying {0} to {1}".format(src, release_dst)
            copy_recursive(src, release_dst)

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
