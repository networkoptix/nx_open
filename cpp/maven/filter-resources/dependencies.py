#!/usr/bin/env python
# -*- coding: utf-8 -*-

import os

TARGET = "${rdep.target}"
RDEP_PATH = os.path.join("${environment.dir}", "rdep")
BUILD_CONFIGURATION = "${build.configuration}"
PLATFORM = "${platform}"

if "windows" in PLATFORM:
    TARGET_DIRECTORY = "${libdir}/${arch}"
else:
    TARGET_DIRECTORY = "${libdir}"


#TODO: This needs a better place
DEPENDENCY_VERSIONS = {
    "qt": "${qt.version}",
    "ffmpeg": "${ffmpeg.version}",
    "openssl": "${openssl.version}",
    "openldap": "${openldap.version}",
    "sasl2": "${sasl2.version}",
    "protobuf": "${protobuf.version}",
    "onvif": "${onvif.version}",
    "sigar": "${sigar.version}",
    "festival": "${festival.version}",
    "festival-vox": "${festival.version}",
    "boost": "${boost.version}",
    "quazip": "${quazip.version}",
    "server-external": "${release.version}"
}

def get_package_version(package):
    version = DEPENDENCY_VERSIONS.get(package, None)
    if version and version.startswith("$"):
        version = None
    return version

def get_versioned_package_name(package):
    version = get_package_version(package)
    if version:
        return package + "-" + version
    return package

def make_list(string):
    return [v.strip() for v in string.split(',') if v]

def get_packages():
    packages = """${rdep.packages}"""
    if '${' in packages:
        return []

    return make_list(packages)

def print_configuration():
    print get_packages()
    print TARGET, BUILD_CONFIGURATION
    print TARGET_DIRECTORY
    print RDEP_PATH

if __name__ == '__main__':
    print_configuration()
