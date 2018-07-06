#!/usr/bin/env python

import os
import sys
import posixpath
import argparse
from rdep import Rdep
import rdep_configure

def print_package(package, status):
    print ">> [{0}]: {1}".format(package, status)

def print_summary(packages):
    for package, status in packages.items():
        print_package(package, status)

def sync_package(rdep, package):
    target, pack = posixpath.split(package)
    if not target:
        print >> sys.stderr, "No target specified for [{0}]".format(package)
        return False, "SKIPPED"

    rdep.targets = [ target ]
    ok = rdep.sync_package(pack)
    status = "OK" if ok else "FAILED"
    return ok, status

def sync(packages, skip_failed = False):
    rdep = Rdep(rdep_configure.REPOSITORY_PATH)
    rdep.fast_check = True
    rdep.load_timestamps_for_fast_check()

    result = {}
    for package in packages:
        ok, status = sync_package(rdep, package)
        result[package] = status
        if not ok and not skip_failed:
            return False, result

    return True, result

if __name__ == '__main__':
    parser = argparse.ArgumentParser()

    parser.add_argument("--skip",
                        help="Skip failed packages.",
                        action="store_true")
    parser.add_argument("packages", nargs='*',
                        help="Packages to sync.",
                        default="")

    args = parser.parse_args()

    ok, result = sync(args.packages, args.skip)
    print_summary(result)

    exit(0 if ok else 1)
