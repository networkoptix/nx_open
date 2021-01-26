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
        "paxton_plugin_references": args.dot_net_framework,
        "customization_pack": f"{args.customization}_v2",
    }

    def sync(package, **kwargs):
        return syncher.sync(package, **kwargs)

    sync("any/cloud_hosts", version="")
    sync("any/customization_pack")
    sync("windows/wix", version="3.11")
    sync("windows/ilmerge", version="")
    sync("windows/paxton_plugin_references")

    syncher.generate_cmake_include(args.cmake_include_file, sync_script_file=__file__)


if __name__ == "__main__":
    main()
