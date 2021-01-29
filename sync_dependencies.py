#!/usr/bin/env python3


from __future__ import print_function
import os
import sys
import argparse
import platform as system_platform
from pathlib import Path
from rdep_cmake import RdepSyncher


def short_release_version(version):
    components = version.split(".")
    try:
        return components[0] + "." + components[1]
    except IndexError:
        print("Invalid release version \"%s\"" % version, file=sys.stderr)
        exit(1)


def determine_package_versions(
    target,
    platform,
    box,
    release_version,
    customization="default",
    debug=False
):
    v = {
        "gcc": "8.1",
        "sdk-gcc": "5.4.0",
        "clang": "8.0.0",
        "qt": "5.11.3",
        "boost": "1.67.0",
        "openssl": "1.0.2q",
        "sigar": "1.7",
        "sasl2": "2.1.26",
        "openal": "1.16",
        "libjpeg-turbo": "1.4.2",
        "festival": "2.4",
        "directx": "JUN2010",
        "pandoc": "2.2.1",
        "cassandra": "2.7.0",
        "doxygen": "1.8.14",
        "verify_globs": "1.0",
        "gstreamer": "1.0",
        "icu": "60.2",
        "android-sdk": "28",
        "android-ndk": "r17",
        "help": customization + "-4.1",
        "server-external": release_version,
        "certificates": customization,
        "customization_pack": f"{customization}_v2",
    }

    # Desktop Linux.
    if platform == "linux" and box == "none" and target not in ("linux_arm32", "linux_arm64"):
        v["festival"] = "2.4-1"
        v["festival-vox"] = "2.4"
        v["ffmpeg"] = "3.1.9-4"
        v["libva"] = "2.6"
        v["intel-media-sdk"] = "19.4.0"
        v["sysroot"] = "xenial-1"
        v["qt"] += "-1"

    if platform == "macosx":
        v["festival"] = "2.1"
        v["ffmpeg"] = "3.1.9-3"
        v["qt"] += "-1"

    if platform == "android":
        v["ffmpeg"] = "3.1.1"
        v["openal"] = "1.17.2"

    if platform == "ios":
        v["ffmpeg"] = "3.1.1"
        v["libjpeg-turbo"] = "1.4.1"

    if target == "linux_arm32":
        v["festival"] = "2.4-1"
        v["festival-vox"] = "2.4"
        v["ffmpeg"] = "3.1.9-6"
        v["ffmpeg-rpi"] = "3.1.9-6"
        v["openssl"] = "1.0.2q-2"
        v["sysroot"] = "jessie"

    if target == "linux_arm64":
        v["festival"] = "2.4-1"
        v["festival-vox"] = "2.4"
        v["ffmpeg"] = "3.1.9-6"
        v["openssl"] = "1.0.2q-2"
        v["sysroot"] = "xenial"
        v["qt"] += "-1"

    if box == "bpi":
        v["festival"] = "2.4-1"
        v["festival-vox"] = "2.4"
        v["ffmpeg"] = "3.1.9-6"
        v["openssl"] = "1.0.2q-2"
        v["sysroot"] = "wheezy"

    if box == "edge1":
        v["ffmpeg"] = "3.1.9-6"
        v["openssl"] = "1.0.2q-2"
        v["sysroot"] = "jessie"

    if "festival-vox" not in v:
        v["festival-vox"] = v["festival"]

    if platform == "windows":
        v["ffmpeg"] = "3.1.9-3"
        v["intel-media-sdk"] = "2019R1"
        v["glew"] = "2.1.0"
        v["qt"] += "-1"
        if debug:
            for package in ("qt", "festival", "openal", "openssl", "sigar", "icu"):
                v[package] += "-debug"

    return v


def sync_dependencies(target, syncher, platform, arch, box, release_version, options={}):
    have_mediaserver = platform not in ("android", "ios", "macosx")
    have_desktop_client = platform in ("windows", "macosx") \
        or (platform == "linux" and (box == "none" or arch == "arm64"))
    have_mobile_client = have_desktop_client or platform in ("android", "ios")

    sync = syncher.sync

    if platform == "linux":
        if box == "bpi":
            sync("bpi/gcc")
        elif arch == "arm64":
            sync("linux_arm64/gcc")
        else:
            sync("linux-%s/gcc" % arch)

        if box == "bpi":
            sync("linux_arm32/sdk-gcc")
        elif box == "edge1":
            sync("linux_arm32/sdk-gcc")
        elif arch == "arm":
            sync("linux_arm32/sdk-gcc")
        elif arch == "arm64":
            sync("linux_arm64/sdk-gcc")
        else:
            sync("linux-x64/sdk-gcc")
        if box == "none" and target not in ("linux_arm32", "linux_arm64"):
            sync("libva")
            sync("intel-media-sdk")

        if options.get("clang"):
            sync("linux/clang")
    elif platform == "android":
        if "ANDROID_HOME" not in os.environ:
            sync("android/android-sdk")
        if "ANDROID_NDK" not in os.environ:
            sync("android/android-ndk")

    sync("any/cloud_hosts", version="")
    sync("qt")
    sync("any/boost")

    sync("any/detection_plugin_interface", version="")

    if box in ("bpi", "edge1"):
        sync("linux_arm32/openssl")
    else:
        sync("openssl")

    if box in ("bpi", "edge1"):
        sync("linux_arm32/ffmpeg")
    else:
        if (platform, arch, box) == ("linux", "arm", "none"):
            sync("rpi/ffmpeg-rpi", do_not_include=True)
        sync("ffmpeg")

    if platform == "linux":
        sync("sysroot")

    if platform in ("android", "windows") or box == "bpi":
        sync("openal")

    if platform == "windows":
        sync("icu")
        sync("directx")
        sync("vmaxproxy", version="2.1")
        sync("ucrt", version="10-redist")
        sync("msvc", version="2017-redist")
        sync("windows/wix", version="3.11")
        sync("windows/ilmerge", version="")
        sync("intel-media-sdk")
        sync("glew")

    if platform in ("windows", "linux"):
        sync("%s/pandoc" % platform)

    if box == "edge1":
        sync("cpro", version="1.0.1")
        sync("gdb", version="")

    if arch == "x64":
        sync("cassandra")

    if have_desktop_client:
        sync("any/help")

    if have_desktop_client or have_mobile_client:
        sync("any/roboto-fonts", version="")

    if (have_mediaserver or have_desktop_client) and box != "edge1":
        sync("festival")
        if syncher.versions["festival-vox"] != "system":
            sync("any/festival-vox")

    if platform in ("android", "ios"):
        sync("libjpeg-turbo")

    if platform in ("macosx", "ios"):
        sync("any/certificates")

    if have_mediaserver:
        if platform == "windows" or (platform == "linux" and arch == "x64"):
            sync("vms_benchmark-dev", version="")
            sync("any/vms_benchmark_aux_files", version="")
            sync("dw_edge_analytics_plugin", version="")

        sync("sigar")
        sync("any/apidoctool", version="2.1")
        if not sync("any/server-external", optional=True):
            sync("any/server-external", version=release_version)

        if box == "edge1":
            sync("openldap", version="2.4.42-1")
            sync("sasl2")

    if have_mediaserver or have_desktop_client:
        sync("%s/doxygen" % platform)
        sync("any/update_verification_keys", version="")

    sync("any/root-certificates", version="")
    sync("any/customization_pack")

    build_platform = system_platform.system()
    if build_platform == 'Windows':
        build_utils_platform = 'windows'
    elif build_platform == 'Linux':
        build_utils_platform = 'linux'
    elif build_platform == 'Darwin':
        build_utils_platform = 'macos'
    else:
        build_utils_platform = platform
    sync("%s/verify_globs" % build_utils_platform)


def parse_target(target):
    components = target.split("-")

    platform = None
    arch = None
    box = "none"

    if target == "linux_arm32":
        platform = "linux"
        arch = "arm"
        box = "none"
    elif target == "linux_arm64":
        platform = "linux"
        arch = "arm64"
        box = "none"
    elif len(components) > 1:
        platform, arch = components

        if platform == "android":
            box = platform
    else:
        box = target

        if box == "ios":
            platform = "ios"
        else:
            platform = "linux"
            arch = "arm"

    return platform, arch, box


def parse_options(options_list):
    options = {}

    for item in options_list:
        try:
            name, value = item.split("=")
            if value == "True":
                value = True
            elif value == "False":
                value = False
            options[name] = value
        except ValueError:
            options[item] = True

    return options


def parse_overrides(overrides_list):
    versions = {}
    locations = {}

    for package, value in parse_options(overrides_list).items():
        if type(value) is not str:
            print("Invalid override item \"%s\"" % package, file=sys.stderr)
            exit(1)

        if "/" in value or "\\" in value:
            locations[package] = value
        else:
            versions[package] = value

    return versions, locations


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--packages-dir", required=True, help="Packages directory")
    parser.add_argument("--target", required=True, help="Build target (e.g. linux-x64)")
    parser.add_argument("--debug", action="store_true", help="Prepare for debug build")
    parser.add_argument("--cmake-include-file", default="dependencies.cmake",
        help="CMake include file name")
    parser.add_argument("--use-local", action="store_true",
        help="Don't sync package if a local copy is found")
    parser.add_argument("--release-version", required=True, help="VMS release version")
    parser.add_argument("--customization", default="default", help="VMS customization")
    parser.add_argument("-o", "--overrides", nargs="*", default=[],
        help="Package version or location overrides (e.g. -o ffmpeg=4.0)")
    parser.add_argument("-O", "--options", nargs="*", default=[], help="Additional options")
    parser.add_argument("-f", "--force", action="store_true",
        help="Force running rsync even if package timestamp is up to date")
    parser.add_argument("-v", "--verbose", action="store_true", help="Enable verbose output")

    args, _ = parser.parse_known_args()

    platform, arch, box = parse_target(args.target)

    version_overrides, location_overrides = parse_overrides(args.overrides)
    options = parse_options(args.options)

    syncher = RdepSyncher(
        args.packages_dir,
        verbose=args.verbose,
        sync_timestamps=options.get("sync-timestamps"))

    syncher.cmake_include_file = args.cmake_include_file
    syncher.rdep_target = args.target
    syncher.versions = determine_package_versions(
        args.target,
        platform,
        box,
        args.release_version,
        customization=args.customization,
        debug=args.debug
    )
    syncher.versions.update(version_overrides)
    syncher.locations = location_overrides
    syncher.rdep.force = args.force
    syncher.rdep.fast_check = not args.force
    syncher.use_local = args.use_local and not args.force

    sync_dependencies(args.target, syncher, platform, arch, box, args.release_version, options)

    syncher.generate_cmake_include(
        cmake_include_file=syncher.cmake_include_file, sync_script_file=__file__)


if __name__ == "__main__":
    main()
