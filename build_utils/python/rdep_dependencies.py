#!/usr/bin/env python

import os, sys, shutil
import posixpath
import argparse
import tempfile
import filelock
import ConfigParser
import glob
import time
import distutils.spawn
from rdep import Rdep
import rdep_config
import platform_detection
from fsutil import copy_recursive

REPOSITORY_PATH = os.path.join(os.getenv("environment"), "packages")
SYNC_URL = "rsync://enk.me/buildenv/rdep/packages"
if time.timezone == 28800:
    SYNC_URL = "rsync://la.hdw.mx/buildenv/rdep/packages"
PUSH_URL = "rsync@enk.me:buildenv/rdep/packages"
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

def copy_package(package_dir, destination, debug):
    copy_list = rdep_config.PackageConfig(package_dir).get_copy_list()

    for dst, sources in copy_list.items():
        if dst in [ "bin", "lib" ]:
            dst = os.path.join(dst, "debug" if debug else "release")
        dst_dir = os.path.join(destination, dst)

        for src in sources:
            src = os.path.join(package_dir, src)
            print "Copying {0} to {1}".format(src, dst_dir)
            if not os.path.isdir(dst_dir):
                os.makedirs(dst_dir)
            copy_recursive(src, dst_dir)

    return True

def get_package_for_configuration(rdep, package, target_dir, debug):
    synched = False
    installed = False

    full_name = package + "-debug" if debug else package

    repo_ts = get_package_timestamp(target_dir, full_name)
    install_ts = get_package_timestamp(target_dir, full_name, install_time = True)
    location = None
    installed = False

    if repo_ts != None:
        if install_ts != None and install_ts == repo_ts:
            installed = True
        location = rdep.locate_package(full_name)
        if installed and not location and debug:
            location = rdep.locate_package(package)

    if repo_ts == None or not location:
        print "Fetching package {0}".format(full_name)
        if rdep.sync_package(full_name):
            location = rdep.locate_package(full_name)
        elif rdep.sync_package(package):
            location = rdep.locate_package(package)
        else:
            return False

        if location:
            repo_ts = rdep_config.PackageConfig(location).get_timestamp(None)
            set_package_synctime(target_dir, full_name, repo_ts)

    if not location or repo_ts == None:
        print "Could not locate {0}".format(package)
        return False

    deps_file = os.path.join(location, package + get_deps_file_suffix())
    if not os.path.isfile(deps_file):
        deps_file = locate_deps_file(location)
    if deps_file and os.path.isfile(deps_file):
        append_deps(deps_file, debug)

    if install_ts != None and install_ts == repo_ts:
        installed = True

    if not installed:
        print "Copying package {0} into {1}".format(full_name, target_dir)
        copy_package(location, target_dir, debug)
        set_package_synctime(target_dir, full_name, repo_ts, install_time = True)

    # Get real target from location
    target, _ = os.path.split(location)
    _, target = os.path.split(target)

    return target

def get_package(rdep, target, package, target_dir, debug = False):
    lock_file = os.path.join(tempfile.gettempdir(), "rdep.lock")
    pack = None

    with filelock.Lock(lock_file) as lock:
        for pack in package.split("|"):
            explicit_target, pack = posixpath.split(pack)
            rdep.targets = [ explicit_target ] if explicit_target else [ target ] + platform_detection.get_alternative_targets(target)

            found_target = get_package_for_configuration(rdep, pack, target_dir, False)
            if found_target:
                break

        if not found_target:
            return False

        if debug:
            rdep.targets = found_target if type(found_target) is list else [ found_target ]
            if not get_package_for_configuration(rdep, pack, target_dir, True):
                return False

    return True

def get_dependencies(target, packages, target_dir, debug = False, deps_file = "qmake"):
    global GENERATE_CMAKE_DEPS

    GENERATE_CMAKE_DEPS = deps_file == "cmake"

    if not packages:
        print "No dependencies found"
        return

    if not os.path.isdir(REPOSITORY_PATH):
        os.makedirs(REPOSITORY_PATH)

    config = rdep_config.RepositoryConfig(REPOSITORY_PATH)
    if not config.get_url():
        config.set_url(SYNC_URL)
    if not config.get_push_url(None):
        config.set_push_url(PUSH_URL)

    global_config = rdep_config.RdepConfig()
    if not global_config.get_rsync():
        if not distutils.spawn.find_executable("rsync"):
            rsync = os.path.join(os.getenv("environment"), "rsync-win32", "rsync.exe")
            if not os.path.isfile(rsync):
                print "Cannot find rsync executable. Please specify it in .rderc"
                exit(1)
            global_config.set_rsync(rsync)
    if not global_config.get_name():
        homedir = os.path.join(os.path.expanduser("~"))
        hg_config_file = os.path.join(homedir, ".hgrc")
        if not os.path.isfile(hg_config_file):
            hg_config_file = os.path.join(homedir, "Mercurial.ini")
        if os.path.isfile(hg_config_file):
            hg_config = rdep_config.ConfigHelper(hg_config_file)
            username = hg_config.get_value("ui", "username", "").strip()
            if username:
                global_config.set_name(username)

    if not os.path.isdir(target_dir):
        os.makedirs(target_dir)

    # Clear dependenciy files
    for debug in [ False, True ]:
        open(get_deps_file(debug), "w").close()

    rdep = Rdep(REPOSITORY_PATH)

    for package in packages:
        if not get_package(rdep, target, package, target_dir, debug):
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
