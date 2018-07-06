#!/usr/bin/env python

import os
import posixpath
import rdep_dependencies
import platform_detection

#TODO: This needs a better place
DEPENDENCY_VERSIONS = {
    "qt": "${qt.version}",
    "gcc": "${gcc.version}",
    "gtest": "${gtest.version}",
    "gmock": "${gmock.version}",
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
    "openal": "${openal.version}",
    "libjpeg-turbo": "${libjpeg-turbo.version}",
    "vmux": "${vmux.version}",
    "cpro": "${cpro.version}",
    "server-external": "${server-external.version}",
    "help": "${help.version}"
}

def get_package_version(package):
    version = DEPENDENCY_VERSIONS.get(package, None)
    if version and version.startswith("$"):
        version = None
    return version

def get_versioned_package_name(package):
    result = []
    for pack in package.split('|'):
        target, pack = posixpath.split(pack)
        version = get_package_version(pack)
        if version:
            pack += "-" + version
        result.append(posixpath.join(target, pack) if target else pack)
    return "|".join(result)

def get_packages(target):
    packages = """${rdep.global.packages} ${rdep.packages}"""

    target_platform = platform_detection.get_platform_for_target(target)

    if target_platform == "windows":
        packages += """ ${rdep.windows.packages}"""
    elif target_platform == "linux":
        packages += """ ${rdep.linux.packages}"""
        if target in [ "bpi", "rpi", "isd_s2", "edge1" ]:
            packages += """ ${rdep.linux.arm.packages}"""
    elif target_platform == "macosx":
        packages += """ ${rdep.mac.packages}"""
    elif target_platform == "android":
        packages += """ ${rdep.android.packages}"""
    elif target_platform == "ios":
        packages += """ ${rdep.ios.packages}"""

    if '{' in packages:
        return []

    return [get_versioned_package_name(package) for package in packages.split()]

if __name__ == '__main__':
    target = "${rdep.target}"
    debug = "${build.configuration}" == "debug"
    target_dir = "${libdir}"
    packages = get_packages(target)

    print "------------------------------------------------------------------------"
    print "Get packages"
    print "------------------------------------------------------------------------"
    print "Target:", target
    print "Target dir:", target_dir
    print "Debug:", debug
    print "Packages:", packages

    rdep_dependencies.get_dependencies(target, packages, target_dir, debug)
