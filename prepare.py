#!/usr/bin/env python2

import argparse
import os
import sys
import subprocess
import glob
import re
import ConfigParser

RDEP_PATH = os.path.join(sys.path[0], "build_utils", "python")
sys.path.append(RDEP_PATH)
import rdep_configure
import sync
import platform_detection

QBS = os.environ["QBS"]
if not QBS:
    QBS = "qbs"

gcc_version_for_target = {
    "bpi": "4.8.3",
    "rpi": "4.9.3",
    "tx1": "4.8.4",
    "edge1": "4.9.3"
}

def package_name(target, package, version):
    return target + "/" + package + "-" + version

def profile_name(target, package, version):
    return "vms-" + target + "-" + package + "-" + version.replace(".", "-")

def check_profile(profile):
    out = subprocess.check_output([QBS, "config", "--list", "profiles." + profile])
    return True if out else False

def get_compiler_path(target, version):
    path = os.path.join(rdep_configure.REPOSITORY_PATH, target, "gcc-" + version, "bin")
    entries = glob.glob(path + "/arm-linux-*-gcc")
    if len(entries) != 1:
        print >> sys.stderr, "Could not find gcc in", path
        exit(1)
    return entries[0]

def get_qmake_path(target, version):
    path = os.path.join(rdep_configure.REPOSITORY_PATH, target, "qt-" + version, "bin", "qmake")
    if not os.path.isfile(path):
        print >> sys.stderr, "Could not find qmake in", path
        exit(1)
    return path

def create_qbs_wrapper(project_dir, profile):
    if os.path.samefile(project_dir, os.getcwd()):
        return

    file_name = "qbs.ini"
    config = ConfigParser.ConfigParser()
    if os.path.isfile(file_name):
        config.read(file_name)

    if not config.has_section("General"):
        config.add_section("General")
    if not config.has_section("Properties"):
        config.add_section("Properties")

    config.set("General", "project", project_dir)
    config.set("Properties", "profile", profile)

    with open(file_name, "w") as file:
        config.write(file)

    print "Now you can run run_qbs.py to build the project"

def prepare(target, reconfigure, project_dir):
    qt_version = "5.6.2"
    if target.startswith("macosx"):
        qt_version = "5.6.1"
    qt_profile = profile_name(target, "qt", qt_version)
    gcc_version = gcc_version_for_target.get(target)

    packages = [ package_name(target, "qt", qt_version) ]
    if gcc_version:
        packages.append(package_name(target, "gcc", gcc_version))

    ok, _ = sync.sync(packages)
    if not ok:
        return False

    compiler_profile = None
    compiler_path = None
    if target.startswith("windows"):
        compiler_profile = "msvc2015"
    elif target.startswith("linux"):
        compiler_profile = "gcc"
    elif target.startswith("macosx"):
        compiler_profile = "xcode-macosx-x86_64"
    elif gcc_version:
        compiler_profile = profile_name(target, "gcc", gcc_version)
        compiler_path = get_compiler_path(target, gcc_version)
    else:
        print >> sys.stderr, "Compiler is not defined for target", target

    if reconfigure or (compiler_path and not check_profile(compiler_profile)):
        if compiler_path:
            print "Configuring compiler profile:", compiler_path, "->", compiler_profile
            subprocess.call([QBS, "setup-toolchains", compiler_path, compiler_profile])
        else:
            print "Detecting compiler profiles:"
            subprocess.call([QBS, "setup-toolchains", "--detect"])

    if reconfigure or (not check_profile(qt_profile)):
        qmake_path = get_qmake_path(target, qt_version)
        print "Configuring qt profile:", qmake_path , "->", qt_profile
        subprocess.call([QBS, "setup-qt", qmake_path, qt_profile])
        subprocess.call([QBS, "config", "profiles." + qt_profile + ".baseProfile", compiler_profile])

    print ">> Prepared for profile", qt_profile

    if project_dir:
        create_qbs_wrapper(project_dir, qt_profile)

    return True

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("-t", "--target",
        help="Build target.",
        default=platform_detection.detect_target())
    parser.add_argument("--reconfigure",
        help="Configure profiles even if they are already created.",
        action="store_true")
    args = parser.parse_args()

    if not args.target in platform_detection.supported_targets:
        print >> sys.stderr, "Unsupported target", args.target

    if not rdep_configure.configure():
        exit(1)

    if not prepare(args.target, args.reconfigure, sys.path[0]):
        exit(1)

if __name__ == "__main__":
    main()
