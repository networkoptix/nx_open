#!/usr/bin/env python

import argparse
import os
import time
import ConfigParser
import subprocess
import shutil
import tempfile
import posixpath
import distutils.spawn

from fsutil import copy_recursive
from platform_detection import *

ROOT_CONFIG_NAME = ".rdep"
PACKAGE_CONFIG_NAME = ".rdpack"
ANY_KEYWORD = "any"
DEBUG_SUFFIX = "-debug"
RSYNC = [ "rsync" ]
RSYNC_CHMOD_ARG = None
if detect_platform() == "windows":
    RSYNC_CHMOD_ARG = "--chmod=ugo=rwx"
    if not distutils.spawn.find_executable("rsync"):
        RSYNC = [os.path.join(os.getenv("environment"), "rsync-win32", "rsync.exe")]

DEFAULT_SYNC_URL = "rsync://enk.me/buildenv/rdep/packages"

verbose = False

def is_relative_to(path, parent):
    return path.startswith(parent)

def verbose_message(message):
    if verbose:
        print message

def verbose_rsync(command):
    verbose_message("Executing rsync:\n{0}".format(" ".join(command)))

def find_root(path, file_name):
    while not os.path.isfile(os.path.join(path, file_name)):
        nextpath = os.path.dirname(path)
        if path == nextpath:
            return None
        else:
            path = nextpath

    return path

def get_repository_root():
    root = find_root(os.getcwd(), ROOT_CONFIG_NAME)
    if not root:
        root = os.getenv("NX_REPOSITORY")
        if root:
            root = os.path.abspath(root)
            if root.endswith("/"):
                root = root[:-1]
    return root


def get_root_config_value(path, section, option):
    config_file = os.path.join(path, ROOT_CONFIG_NAME)
    if not os.path.isfile(config_file):
        return None

    config = ConfigParser.ConfigParser()
    config.read(config_file)
    if not config.has_option(section, option):
        return None

    return config.get(section, option)

def get_sync_url(path):
    return get_root_config_value(path, "General", "url")

def get_ssh_args(path):
    return get_root_config_value(path, "General", "ssh")

def package_config_path(path):
    return os.path.join(path, PACKAGE_CONFIG_NAME)

def check_repository(path):
    return bool(get_sync_url(path))

def detect_repository():
    root = get_repository_root()
    if not root:
        print "Could not find repository root"
        exit(1)

    url = get_sync_url(root)
    if not url:
        print "Could not find sync url for {0}".format(root)
        exit(1)

    return root, url

def detect_package_and_target(root, path):
    if not is_relative_to(path, root):
        return None, None

    path, target = os.path.split(path)
    package = None

    while path != root and target:
        package = target
        path, target = os.path.split(path)

    if path == root:
        if target in supported_targets:
            return target, package

    return None, None

def init_repository(path, url):
    if not os.path.isdir(path):
        print "Directory does not exists: {0}".format(path)
        return False

    try:
        with open(os.path.join(path, ROOT_CONFIG_NAME), "w") as config_file:
            config = ConfigParser.ConfigParser()
            config.add_section("General")
            config.set("General", "url", url)
            config.write(config_file)
    except:
        print "Could not init repository at {0}".format(path)
        return False

    print "Initialized repository at {0}".format(path)
    print "The repository will use URL = {0}".format(url)

    return True

def get_timestamp_from_package_config(file_name):
    if not os.path.isfile(file_name):
        return None

    config = ConfigParser.ConfigParser()
    config.read(file_name)

    if not config.has_option("General", "time"):
        return 0

    return config.getint("General", "time")

def get_package_timestamp(path):
    return get_timestamp_from_package_config(os.path.join(path, PACKAGE_CONFIG_NAME))

def update_package_timestamp(path, timestamp = None):
    file_name = os.path.join(path, PACKAGE_CONFIG_NAME)

    if timestamp == None:
        timestamp = time.time()

    config = ConfigParser.ConfigParser()
    if os.path.isfile(file_name):
        config.read(file_name)

    if not config.has_section("General"):
        config.add_section("General")

    config.set("General", "time", int(timestamp))

    with open(file_name, "w") as file:
        config.write(file)

SYNC_NOT_FOUND = 0
SYNC_FAILED = 1
SYNC_SUCCESS = 2

def get_rsync_command(source,
                      destination,
                      rdpack_file = False,
                      show_progress = True,
                      additional_args = []):

    command = list(RSYNC)
    command.append("--archive")
    if not rdpack_file:
        command.append("--delete")

    if show_progress:
        command.append("--progress")

    if RSYNC_CHMOD_ARG and not ":" in destination:
        command.append(RSYNC_CHMOD_ARG)

    command.append(source)
    command.append(destination)

    return command

# Workaround against rsync bug: all paths with semicolon are counted as remote, so 'rsync rsync://server/path c:\test\path' won't work on windows
def posix_path(path):
    if len(path) > 1 and path[1] == ':':
        drive_letter = path[0].lower()
        return "/cygdrive/{0}{1}".format(drive_letter, path[2:].replace("\\", "/"))

    return path

def fetch_package_config(url, file_name):
    command = get_rsync_command(
            url,
            posix_path(file_name),
            show_progress = False,
            rdpack_file = True)

    verbose_rsync(command)

    with open(os.devnull, "w") as fnull:
        return subprocess.call(command, stderr = fnull) == 0

    return False

def try_sync(root, url, target, package, force):
    src = posixpath.join(url, target, package)
    dst = os.path.join(root, target, package)

    verbose_message("root {0}\nurl {1}\ntarget {2}\npackage {3}\nsrc {4}\ndst {5}".format(root, url, target, package, src, dst))

    config_file = tempfile.mktemp()
    if not fetch_package_config(posixpath.join(src, PACKAGE_CONFIG_NAME), config_file):
        return SYNC_NOT_FOUND

    newtime = get_timestamp_from_package_config(config_file)
    if newtime == None:
        os.remove(config_file)
        return SYNC_NOT_FOUND

    time = get_package_timestamp(dst)
    if newtime == time and not force:
        os.remove(config_file)
        return SYNC_SUCCESS

    if not os.path.isdir(dst):
        os.makedirs(dst)

    command = get_rsync_command(
            src + "/",
            posix_path(dst),
            additional_args = [ "--exclude", PACKAGE_CONFIG_NAME]
    )

    verbose_rsync(command)

    if subprocess.call(command) != 0:
        os.remove(config_file)
        return SYNC_FAILED

    verbose_message("Moving {0} to {1}".format(config_file, os.path.join(dst, PACKAGE_CONFIG_NAME)))
    shutil.move(config_file, os.path.join(dst, PACKAGE_CONFIG_NAME))

    return SYNC_SUCCESS

def sync_package(root, url, target, package, debug, force):
    print "Synching {0}...".format(package)

    to_remove = None

    ret = try_sync(root, url, target, package, force)
    if ret == SYNC_NOT_FOUND:
        path = os.path.join(root, target, package)
        if os.path.isdir(path):
            to_remove = path

        target = ANY_KEYWORD

        ret = try_sync(root, url, target, package, force)
        if ret == SYNC_NOT_FOUND:
            path = os.path.join(root, target, package)
            print "Could not find {0}".format(package)
            return False

    if to_remove:
        print "Removing local {0}".format(to_remove)
        shutil.rmtree(to_remove)
        to_remove = None

    if ret == SYNC_FAILED:
        print "Sync failed for {0}".format(package)
        return False

    if debug:
        ret = try_sync(root, url, target, package + DEBUG_SUFFIX, force)

        if ret == SYNC_NOT_FOUND:
            path = os.path.join(root, target, package + DEBUG_SUFFIX)
            if os.path.isdir(path):
                to_remove = path
        elif ret == SYNC_FAILED:
            print "Sync failed for {0}".format(package + DEBUG_SUFFIX)
            return False

    if to_remove:
        print "Removing local {0}".format(to_remove)
        shutil.rmtree(to_remove)

    print "Done {0}".format(package)
    return True

def sync_packages(root, url, target, packages, debug, force):
    global RSYNC

    if not url.startswith("rsync://"):
        ssh = get_ssh_args(root)
        if ssh:
            RSYNC += [ "-e", ssh ]

    success = True

    for package in packages:
        if not sync_package(root, url, target, package, debug, force):
            success = False

    return success

def upload_package(root, url, target, package):
    print "Uploading {0}...".format(package)

    remote = posixpath.join(url, target, package)
    local = os.path.join(root, target, package)

    update_package_timestamp(local)

    command = get_rsync_command(
            posix_path(local) + "/",
            remote,
            additional_args = [ "--exclude", PACKAGE_CONFIG_NAME]
    )

    verbose_rsync(command)

    if subprocess.call(command) != 0:
        print "Could not upload {0}".format(package)
        return False

    command = get_rsync_command(
            posix_path(os.path.join(local, PACKAGE_CONFIG_NAME)),
            remote,
            rdpack_file = True,
            show_progress = False
    )

    verbose_rsync(command)

    if subprocess.call(command) != 0:
        print "Could not upload {0}".format(package)
        return False

    print "Done {0}".format(package)
    return True

def upload_packages(root, url, target, packages, debug = False):
    global RSYNC

    if not url.startswith("rsync://"):
        ssh = get_ssh_args(root)
        if ssh:
            RSYNC += [ "-e", ssh ]

    success = True

    if not packages:
        print "No packages to upload"
        return True

    for package in packages:
        package_name = package + DEBUG_SUFFIX if debug else package
        if not upload_package(root, url, target, package_name):
            success = False

    if success:
        print "Uploaded successfully"
    return success

def locate_package(root, target, package, debug = False):
    if debug:
        path = os.path.join(root, target, package + DEBUG_SUFFIX)
        if os.path.exists(package_config_path(path)):
            return path

    path = os.path.join(root, target, package)
    if os.path.exists(package_config_path(path)):
        return path

    any_target = ANY_KEYWORD

    if debug:
        path = os.path.join(root, any_target, package + DEBUG_SUFFIX)
        if os.path.exists(package_config_path(path)):
            return path

    path = os.path.join(root, any_target, package)
    if os.path.exists(package_config_path(path)):
        return path

    return None

def fetch_packages(root, url, target, packages, debug = False, force = False):
    print "Ready to work on {0}".format(target)
    print "Repository root dir: {0}".format(root)

    if not packages:
        print "No packages to sync"
        return False

    if not sync_packages(root, url, target, packages, debug, force):
        return False

    return True

def print_path(root, target, packages, debug):
    if not packages or len(packages) != 1:
        exit(1)

    path = locate_package(root, target, packages[0], debug)
    if not path:
        exit(1)
    print(path)

def get_copy_list(package_dir):
    config = ConfigParser.ConfigParser()
    config.read(os.path.join(package_dir, PACKAGE_CONFIG_NAME))

    if not config.has_section("Copy"):
        return { "bin": [ "bin/*" ], "lib": [ "lib/*.so*", "lib/*.dylib*" ] }

    result = {}
    for key, value in config.items("Copy"):
        result[key] = value.split()

    return result

def copy_package(root, target, package, destination, debug = False):
    package_dir = locate_package(root, target, package, debug)
    if not package_dir:
        print "Could not locate package {0}".format(package)
        return False

    copy_list = get_copy_list(package_dir)

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

def copy_packages(root, target, packages, destination, debug = False):
    success = True

    if not packages:
        print "No packages to copy"
        return True

    for package in packages:
        if not copy_package(root, target, package, destination, debug):
            success = False

    if success:
        print "Copied successfully"
    return success

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("-t", "--target",       help="Target classifier.",  default=detect_target())
    parser.add_argument("-d", "--debug",        help="Sync debug version.",                 action="store_true")
    parser.add_argument("-f", "--force",        help="Force sync.",                         action="store_true")
    parser.add_argument("-u", "--upload",       help="Upload package to the repository.",   action="store_true")
    parser.add_argument("-v", "--verbose",      help="Additional debug output.",            action="store_true")
    parser.add_argument("--print-path",         help="Print package dir and exit.",         action="store_true")
    parser.add_argument("--copy", metavar="DIR", help="Copy package resources")
    parser.add_argument("--init", metavar="URL", help="Init repository in the current dir with the specified URL.")
    parser.add_argument("packages", nargs='*',  help="Packages to sync.",   default="")

    args = parser.parse_args()

    if args.init:
        success = init_repository(os.getcwd(), args.init)
        exit(0 if success else 1)

    root, url = detect_repository()

    target = args.target
    if not target in supported_targets and target != ANY_KEYWORD:
        print "Unsupported target {0}".format(target)
        print "Supported targets:\n{0}".format("\n".join(supported_targets))
        exit(1)

    packages = args.packages
    if root:
        detected_target, package = detect_package_and_target(root, os.getcwd())
        if detected_target:
            target = detected_target
        if not packages and package:
            packages = [ package ]

    if args.verbose:
        global verbose
        verbose = True

    success = True

    if args.print_path:
        success = print_path(root, target, packages, args.debug)
    elif args.upload:
        success = upload_packages(root, url, target, packages, args.debug)
    elif args.copy:
        success = copy_packages(root, target, packages, args.copy, args.debug)
    else:
        success = fetch_packages(root, url, target, packages, args.debug, args.force)

    exit(0 if success else 1)

if __name__ == "__main__":
    main()
