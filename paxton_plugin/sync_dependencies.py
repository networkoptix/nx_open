#!/usr/bin/env python

from __future__ import print_function
import argparse
from rdep_cmake import RdepSyncher


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--packages-dir", required=True, help="Packages directory")
    parser.add_argument("--dot-net-framework")
    parser.add_argument("--customization", default="default", help="VMS customization")
    parser.add_argument("--cmake-include-file", default="dependencies.cmake",
                        help="CMake include file name")
    args = parser.parse_args()

    syncher = RdepSyncher(args.packages_dir)
    syncher.versions = {
        "paxton_plugin_references": args.dot_net_framework
    }

    def sync(package, **kwargs):
        return syncher.sync(package, **kwargs)

    sync("any/cloud_hosts")
    sync("windows/wix-3.11", path_variable="wix_directory")
    sync("windows/ilmerge", path_variable="ilmerge_directory")
    sync("windows/paxton_plugin_references", path_variable="references_directory")

    syncher.generate_cmake_include(args.cmake_include_file)


if __name__ == "__main__":
    main()
