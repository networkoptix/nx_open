#!/usr/bin/env python
"""
Runs through beta__builds directories tree and check if all artifacts are published
See: https://networkoptix.atlassian.net/wiki/display/SD/Installer+Filenames
"""

import sys
import argparse
import os
import requests
from common_module import init_color,info,green,warn,err,separator

from artifact import get_artifacts, AppType

verbose = False

root_path = "http://beta.networkoptix.com/beta-builds/daily"

def check_file_exists(build_path, customization, folder, name):
    full_path = '/'.join([root_path, build_path, customization, folder, name])
    if verbose:
        info("Requesting {0}".format(full_path))
    response = requests.head(full_path)
    if verbose:
        info("Response {0}".format(response.status_code))
    return response.status_code == requests.codes.ok

def get_build_path(build):
    if verbose:
        info("Requesting {0}".format(root_path))
    response = requests.get(root_path)
    before, result, after = response.text.partition(str(build))
    if not result:
        return None
    pos = 1
    while pos < len(after) and after[pos] != '/':
        pos += 1
    result += after[:pos]
    return result

def get_artifact_folder(artifact, build):
    if artifact.extension == "apk":
        return "android"
    if artifact.extension == "ipa":
        return "ios"
    if artifact.extension == "tar.gz":
        return "arm"
    if artifact.extension == "dmg":
        return "macos"
    if artifact.extension == "exe" or artifact.extension == "msi":
        return "windows"
    if artifact.extension == "deb":
        return "linux"
    if artifact.extension == "zip":
        return '/'.join(["updates", str(build)])

def get_product(customization):
    if customization == "default":
        return "nxwitness"
    return customization

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('-v', '--verbose', action='store_true', help="verbose output")
    parser.add_argument('-c', '--color', action='store_true', help="colorized output")
    parser.add_argument('--customization', metavar="c", help="customization name", required = True)
    parser.add_argument('--build', type=int, help="Build number", metavar="build", required = True)
    parser.add_argument('--cloud', metavar="cloud", default="", help="cloud instance name (default is empty)")
    parser.add_argument('--beta', default="false", help="beta status (default is false)")

    args = parser.parse_args()
    if args.color:
        init_color()

    product = get_product(args.customization)
    version = "3.0.0.{0}".format(args.build)
    beta = args.beta == "true"

    info(
"Validating:\n"
"    Customization: {0}\n"
"    Build: {1}\n"
"    Cloud: {2}\n"
"    Beta: {3}\n"
"    Product: {4}\n"
"    Version: {5}\n"
    .format(
        args.customization,
        args.build,
        args.cloud,
        beta,
        product,
        version
        ))

    global verbose
    verbose = args.verbose
    if verbose:
        warn("Verbose mode")

    build_path = get_build_path(args.build)
    if not build_path:
        err("Build {0} is not found".format(args.build))
        sys.exit(1)

    if verbose:
        info("Build path: {0}".format(build_path))

    count = 0
    missed = 0

    for a in get_artifacts(product, version, beta, args.cloud):
        folder = get_artifact_folder(a, args.build)
        if not check_file_exists(build_path, args.customization, folder, a.name()):
            err("{0} is not found in {1}".format(a, folder))
            missed += 1
        else:
            count += 1

    if missed == 0:
        green("{0} of {0} artifacts found".format(count))
    else:
        err("{0} of {1} artifacts found".format(count, missed + count))
        sys.exit(1)


if __name__ == '__main__':
    main()