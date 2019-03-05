#!/usr/bin/env python

from __future__ import print_function
import os
import sys
import shutil
import argparse

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

    args = parser.parse_args()

    if not os.path.isdir(args.build_dir):
        return

    print("Clearing {}".format(args.build_dir))

    delete_path(os.path.join(args.build_dir, "distrib"))

    delete_path(os.path.join(args.build_dir, "bin", "mobile_client.app"))
    delete_path(os.path.join(args.build_dir, "client", "mobile_client", "ipa"))

if __name__ == "__main__":
    main()
