#!/usr/bin/env python

from __future__ import print_function
import os
import sys
import shutil
import argparse

CMAKE_CONFIGURATIONS = ["Release", "Debug", "RelWithDebInfo", "MinSizeRel", "Default"]

def log_deletion(path):
    print("Deleting {}".format(path))

def delete_path(path):
    if not os.path.exists(path):
        return

    log_deletion(path)

    if os.path.isdir(path):
        shutil.rmtree(path)
    else:
        os.remove(path)

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--build-dir", help="Build directory.", default=os.getcwd())
    parser.add_argument("--configurations", nargs='*', default=CMAKE_CONFIGURATIONS,
        help="Build configurations to clear (default: {})".format(" ".join(CMAKE_CONFIGURATIONS)))

    args = parser.parse_args()

    if not os.path.isdir(args.build_dir):
        return

    print("Clearing {} for {}".format(args.build_dir, " ".join(args.configurations)))

    delete_path(os.path.join(args.build_dir, "distrib"))

    for configuration in CMAKE_CONFIGURATIONS:
        delete_path(os.path.join(args.build_dir, configuration, "bin", "mobile_client.app"))

if __name__ == "__main__":
    main()
