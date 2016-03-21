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
SYNC_FILE = ".sync"
INSTALL_FILE = ".install"

GENERATE_CMAKE_DEPS = False

def get_package_timestamp(target_dir, package, install_time = False):
    config_file = os.path.join(
        target_dir, INSTALL_FILE if install_time else SYNC_FILE)
    if not os.path.exists(config_file):
        return None

    config = ConfigParser.ConfigParser()
    config.read(config_file)

    if not config.has_option("General", package):
        return None

    return config.getint("General", package)

def set_package_synctime(target_dir, package, timestamp, install_time = False):
    config_file = os.path.join(
        target_dir, INSTALL_FILE if install_time else SYNC_FILE)

    config = ConfigParser.ConfigParser()

    if os.path.isfile(config_file):
        config.read(config_file)

    if not config.has_section("General"):
        config.add_section("General")

    config.set("General", package, timestamp)

    with open(config_file, "w") as file:
        config.write(file)

def configuration_name(debug):
    return "debug" if debug else "release"

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
        if GENERATE_CMAKE_DEPS:
            deps_file = deps_file.replace("\\", "/")
        file.write("include({0})\n".format(deps_file))

def get_package_for_configuration(target, package, target_dir, debug):
    full_name = package if not debug else package + "-debug"

    synched = False
    installed = False

    sync_ts = get_package_timestamp(target_dir, full_name)
    repo_ts = None

    location = rdep.locate_package(REPOSITORY_PATH, target, package, debug)
    if location and sync_ts != None:
        repo_ts = rdep.get_package_timestamp(location)
        if repo_ts != None:
            synched = (repo_ts == sync_ts)

    if not synched:
        print "Fetching package {0} for {1}".format(package, configuration_name(debug))
        rdep.fetch_packages(REPOSITORY_PATH, SYNC_URL, target, [ package ], debug)
        location = rdep.locate_package(REPOSITORY_PATH, target, package, debug)
        repo_ts = rdep.get_package_timestamp(location)
        set_package_synctime(target_dir, full_name, repo_ts)

    if not location:
        print "Could not locate {0}".format(package)
        return False

    install_ts = get_package_timestamp(target_dir, full_name, install_time=True)
    if install_ts != None and install_ts == repo_ts:
        installed = True

    deps_file = os.path.join(location, package + get_deps_file_suffix())
    if not os.path.isfile(deps_file):
        deps_file = locate_deps_file(location)
    if deps_file and os.path.isfile(deps_file):
        append_deps(deps_file, debug)

    if not installed:
        print "Copying package {0} into {1} for {2}".format(package, target_dir, configuration_name(debug))
        rdep.copy_package(REPOSITORY_PATH, target, package, target_dir, debug)
        set_package_synctime(target_dir, full_name, repo_ts, install_time=True)

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

    if not os.path.isdir(target_dir):
        os.makedirs(target_dir)

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
