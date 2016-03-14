#!/usr/bin/env python

import os, sys, shutil
import argparse
import tempfile
import filelock
import ConfigParser
import glob
import time
import rdep
import platform_detection

REPOSITORY_PATH = os.path.join(os.getenv("environment"), "packages")
SYNC_URL = "rsync://hdw.mx/buildenv/rdep/packages"
if time.timezone == 28800:
    SYNC_URL = "rsync://la.hdw.mx/buildenv/rdep/packages"
elif time.timezone == -10800:
    SYNC_URL = "rsync://enk.me/buildenv/rdep/packages"

GENERATE_CMAKE_DEPS = False

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

def configuration_name(debug):
    return "debug" if debug else "release"

def install_dependency(dependency_dir, target_dir, debug):
    print "Installing dependency from {0}".format(dependency_dir)
    bin_dst = os.path.join(target_dir, "bin", configuration_name(debug))
    if not os.path.isdir(bin_dst):
        os.makedirs(bin_dst)

    copy_list = get_copy_list(dependency_dir)

    for entry in copy_list:
        src = os.path.join(dependency_dir, entry)
        print "Copying {0} to {1}".format(src, bin_dst)
        copy_recursive(src, bin_dst)

def get_deps_file_suffix():
    return ".cmake" if GENERATE_CMAKE_DEPS else ".pri"

def locate_deps_file(path):
    if not os.path.isdir(path):
        return None

    result = None

    suffix = get_deps_file_suffix()

    for file in os.listdir(path):
        if file.endswith(suffix):
            if result:
                return None
            else:
                result = os.path.join(path, file)

    return result

def get_deps_file(debug = False):
    base = "dependencies-debug" if debug else "dependencies"
    return base + get_deps_file_suffix()

def append_deps(deps_file, debug = False):
    with open(get_deps_file(debug), "a") as file:
        file.write("include({0})\n".format(deps_file))

def get_package_for_configuration(target, package, target_dir, debug):
    installation_marker = (package if not debug else package + "-debug") + rdep.PACKAGE_CONFIG_NAME
    description_file = os.path.join(target_dir, installation_marker)
    installed = os.path.isfile(description_file)
    if installed:
        location = rdep.locate_package(REPOSITORY_PATH, target, package)
        if not location:
            installed = False

    if not installed:
        print "Fetching package {0} for {1}".format(package, configuration_name(debug))
        rdep.fetch_packages(REPOSITORY_PATH, SYNC_URL, target, [ package ], debug)
        location = rdep.locate_package(REPOSITORY_PATH, target, package)

    if not location:
        print "Could not locate {0}".format(package)
        return False

    deps_file = os.path.join(location, package + ".pri")
    if not os.path.isfile(deps_file):
        deps_file = locate_deps_file(location)
    if deps_file and os.path.isfile(deps_file):
        append_deps(deps_file, debug)

    if not installed:
        print "Copying package {0} into {1} for {2}".format(package, target_dir, configuration_name(debug))
        install_dependency(location, target_dir, debug)

        shutil.copy(os.path.join(location, rdep.PACKAGE_CONFIG_NAME), description_file)

    return True

def get_package(target, package, target_dir, debug = False):
    lock_file = os.path.join(tempfile.gettempdir(), "rdep.lock")

    with filelock.Lock(lock_file) as lock:
        if not get_package_for_configuration(target, package, target_dir, False):
            return False

        if debug:
            if not get_package_for_configuration(target, package, target_dir, True):
                return False

    return True

def get_dependencies(target, packages, target_dir, debug = False, deps_file = "qmake"):
    global SYNC_URL
    global GENERATE_CMAKE_DEPS

    GENERATE_CMAKE_DEPS = deps_file == "cmake"

    if not os.path.isdir(REPOSITORY_PATH):
        os.makedirs(REPOSITORY_PATH)

    if not rdep.check_repository(REPOSITORY_PATH):
        if not rdep.init_repository(REPOSITORY_PATH, SYNC_URL):
            exit(1)

    SYNC_URL = rdep.get_sync_url(REPOSITORY_PATH)

    # Clear dependenciy files
    for debug in [ False, True ]:
        open(get_deps_file(debug), "w").close()

    if not packages:
        print "No dependencies found"
        return

    for package in packages:
        if not get_package(target, package, target_dir, debug):
            print "Cannot get package {0}".format(package)
            sys.exit(1)

if __name__ == '__main__':
    parser = argparse.ArgumentParser()

    parser.add_argument("-t", "--target",       help="Target classifier.",  default = platform_detection.detect_target())
    parser.add_argument("-o", "--target-dir",   help="Target directory.",   default = os.getcwd())
    parser.add_argument("-d", "--debug",        help="Sync debug version.",                 action="store_true")
    parser.add_argument("--deps-file",          help="Generate dependencies file.",  default = "qmake")
    parser.add_argument("packages", nargs='*',  help="Packages to sync.",   default="")

    args = parser.parse_args()

    get_dependencies(args.target, args.packages, args.target_dir, args.debug, args.deps_file)
