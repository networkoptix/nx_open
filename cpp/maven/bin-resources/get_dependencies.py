﻿#!/usr/bin/env python
# -*- coding: utf-8 -*-

import os, sys, shutil
import dependencies
import ConfigParser
import glob

sys.path.insert(0, dependencies.RDEP_PATH)
import rdep
sys.path.pop(0)

debug = "debug" in dependencies.BUILD_CONFIGURATION

#Supports templates such as bin/*.dll
def copy_recursive(src, dst):
    if "*" in src:
        entries = glob.glob(src)
    else:
        entries = [ src ]

    for entry in entries:
        if os.path.isfile(entry):
            shutil.copy2(entry, dst)

        elif os.path.isdir(entry):
            dst_basedir = os.path.join(dst, os.path.basename(entry))

            for dirname, _, filenames in os.walk(entry):
                rel_dir = os.path.relpath(dirname, entry)
                dst_dir = os.path.abspath(os.path.join(dst_basedir, rel_dir))

                if not os.path.isdir(dst_dir):
                    os.makedirs(dst_dir)

                for filename in filenames:
                    shutil.copy2(os.path.join(dirname, filename), dst_dir)

#TODO: this parser functionality should be moved to RDep
def get_copy_list(package_dir):
    config = ConfigParser.ConfigParser()
    config.read(os.path.join(package_dir, rdep.PACKAGE_CONFIG_NAME))

    if not config.has_option("General", "copy"):
        return [ "bin/*" ]

    return config.get("General", "copy").split()

def install_dependency(dependency_dir, target_dir, debug):
    print "Installing dependency from {0}".format(dependency_dir)
    bin_dst = os.path.join(target_dir, "bin", "debug" if debug else "release")
    if not os.path.isdir(bin_dst):
        os.makedirs(bin_dst)

    copy_list = get_copy_list(dependency_dir)

    for entry in copy_list:
        src = os.path.join(dependency_dir, entry)
        print "Copying {0} to {1}".format(src, bin_dst)
        copy_recursive(src, bin_dst)

def get_deps_pri_file(debug = False):
    return "dependencies-debug.pri" if debug else "dependencies.pri"

def append_pri(pri_file, debug = False):
    with open(get_deps_pri_file(debug), "a") as file:
        file.write("include({0})\n".format(pri_file))

def get_package_for_configuration(package, debug):
    target_dir = dependencies.TARGET_DIRECTORY
    versioned = dependencies.get_versioned_package_name(package)

    installation_marker = (package if not debug else package + "-debug") + rdep.PACKAGE_CONFIG_NAME
    description_file = os.path.join(target_dir, installation_marker)
    installed = os.path.isfile(description_file)

    if not installed:
        print "Fetching package {0}".format(package)
        rdep.fetch_packages(dependencies.TARGET, [ versioned ], debug)

    location = rdep.locate_package(dependencies.TARGET, versioned)
    if not location:
        print "Could not locate {0}".format(versioned)
        return False

    if not installed:
        print "Copying package {0} into {1}".format(package, target_dir)
        install_dependency(location, target_dir, debug)

        pri_file = os.path.join(location, package + ".pri")
        if os.path.isfile(pri_file):
            append_pri(pri_file, debug)

        shutil.copy(os.path.join(location, rdep.PACKAGE_CONFIG_NAME), description_file)

    return True

def get_package(package):
    if not get_package_for_configuration(package, False):
        return False

    if debug:
        if not get_package_for_configuration(package, True):
            return False

    return True

def copy_packages(packages):
    # Clear dependenciy files
    for debug in [ False, True ]:
        open(get_deps_pri_file(debug), "w").close()

    return all([copy_package(package) for package in packages])


def get_dependencies():
    packages = dependencies.get_packages()
    if not packages:
        print "No dependencies found"
        return

    dependencies.print_configuration()

    for package in packages:
        if not get_package(package):
            print "Cannot get package {0}".format(package)
            sys.exit(1)

if __name__ == '__main__':
    get_dependencies()
