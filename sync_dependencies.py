#!/usr/bin/env python


from __future__ import print_function
import os
import sys
import argparse
from rdep import Rdep
from rdep_cmake import RdepSyncher


def short_release_version(version):
    components = version.split(".")
    try:
        return components[0] + "." + components[1]
    except IndexError:
        print("Invalid release version \"%s\"" % version, file=sys.stderr)
        exit(1)


def determine_package_versions(
    platform,
    box,
    release_version,
    customization="default",
    debug=False
):
    v = {
        "gcc": "7.3.0",
        "clang": "6.0.0",
        "qt": "5.6.2",
        "boost": "1.66.0",
        "openssl": "1.0.2e",
        "ffmpeg": "3.1.1",
        "sigar": "1.7",
        "openldap": "2.4.42",
        "sasl2": "2.1.26",
        "openal": "1.16",
        "libjpeg-turbo": "1.4.2",
        "festival": "2.4",
        "directx": "JUN2010",
        "pandoc": "2.2.1",
        "cassandra": "2.7.0",
        "doxygen": "1.8.14",
        "gstreamer": "1.0",
        "glib": "2.0",
        "deepstream": "0.1",
        "help": customization + "-3.2",
        "server-external": release_version,
        "certificates": customization,
    }

    if platform == "windows":
        v["qt"] = "5.6.1-1"
        v["ffmpeg"] = "3.1.9"

    if platform == "linux" and box == "none":
        v["qt"] = "5.6.2-2"
        v["festival"] = "2.4-1"
        v["festival-vox"] = "2.4"
        v["sysroot"] = "xenial"
        v["ffmpeg"] = "3.1.9"

    if platform == "linux" and box != "none":
        if box == "tx1":
            v["gcc"] = "7.2.0"
        else:
            v["gcc"] = "linaro-7.2.1"

    if platform == "macosx":
        v["qt"] = "5.6.3"
        v["ffmpeg"] = "3.1.9"
        v["openssl"] = "1.0.2e-2"
        v["festival"] = "2.1"

    if platform == "android":
        v["qt"] = "5.6.2-2"
        v["openssl"] = "1.0.2g"
        v["openal"] = "1.17.2"

    if platform == "ios":
        v["openssl"] = "1.0.1i"
        v["libjpeg-turbo"] = "1.4.1"

    if box in ("bpi", "bananapi", "rpi"):
        v["openssl"] = "1.0.2l-deb9"

    if box in ("bpi", "bananapi"):
        v["qt"] = "5.6.2-1"
        v["festival"] = "2.4-1"
        v["festival-vox"] = "2.4"
        v["sysroot"] = "1"

    if box == "bananapi":
        v["ffmpeg"] = "3.1.1-bananapi"
        v["qt"] = "5.6.3-bananapi"

    if box == "rpi":
        v["qt"] = "5.6.3"
        v["festival"] = "2.4-1"
        v["festival-vox"] = "2.4"

    if box == "edge1":
        v["qt"] = "5.6.3"
        v["openssl"] = "1.0.1f"

    if box == "tx1":
        v["qt"] = "5.6.3"
        v["festival"] = "2.1x"
        v["openssl"] = "1.0.0j"

    if not "festival-vox" in v:
        v["festival-vox"] = v["festival"]

    if platform == "windows" and debug:
        for package in ("festival", "openal", "openssl", "sigar"):
            v[package] += "-debug"

    return v


def sync_dependencies(syncher, platform, arch, box, release_version, options={}):
    have_mediaserver = platform not in ("android", "ios")
    have_desktop_client = platform in ("windows", "macosx") \
        or (platform == "linux" and box in ("none", "tx1"))
    have_mobile_client = have_desktop_client or platform in ("android", "ios")

    sync = syncher.sync

    if platform == "linux":
        if box == "bpi":
            sync("bpi/gcc")
        else:
            sync("linux-%s/gcc" % arch)

        if options.get("clang"):
            sync("linux/clang")
    elif platform == "android":
        if "ANDROID_HOME" not in os.environ:
            sync("android/android-sdk")
        if "ANDROID_NDK" not in os.environ:
            sync("android/android-ndk-r17")

    sync("qt", path_variable="QT_DIR")
    sync("any/boost")

    sync("any/detection_plugin_interface")

    if box in ("rpi", "bpi", "bananapi"):
        sync("linux-arm/openssl")
    else:
        sync("openssl")

    sync("ffmpeg")

    if platform == "linux" and box in ("bpi", "bananapi", "rpi", "tx1", "none"):
        sync("sysroot", path_variable="sysroot_directory")

    if box  == "edge1":
        sync("linux-arm/glib-2.0")
        sync("linux-arm/zlib-1.2")

    if box == "bpi":
        sync("opengl-es-mali")

    if box == "rpi":
        sync("cifs-utils")

    if box == "tx1":
        sync("tegra_video")
        sync("jetpack")
        sync("gstreamer")
        sync("glib")
        sync("deepstream")

    if platform in ("android", "windows") or box == "bpi":
        sync("openal")

    if platform == "linux" and box == "none":
        sync("cifs-utils")
        sync("appserver-2.2.1")

    if platform == "windows":
        sync("directx")
        sync("vcredist-2015", path_variable="vcredist_directory")
        sync("vmaxproxy-2.1")
        sync("windows/wix-3.11", path_variable="wix_directory")
        sync("windows/signtool", path_variable="signtool_directory")

    if platform in ("windows", "linux"):
        sync("%s/pandoc" % platform, path_variable="pandoc_directory")

    if box == "edge1":
        sync("cpro-1.0.0-2")
        sync("gdb")

    if arch == "x64":
        sync("cassandra")

    if have_desktop_client:
        sync("any/help", path_variable="help_directory")

    if have_desktop_client or have_mobile_client:
        sync("any/roboto-fonts", path_variable="fonts_directory")

    if (have_mediaserver or have_desktop_client) and box != "edge1":
        sync("festival")
        if syncher.versions["festival-vox"] != "system":
            sync("any/festival-vox", path_variable="festival_vox_directory")

    if platform in ("android", "ios"):
        sync("libjpeg-turbo")

    if have_mediaserver:
        sync("any/nx_sdk-1.7.1")
        sync("any/nx_storage_sdk-1.7.1")
        sync("sigar")
        sync("any/apidoctool-2.0", path_variable="APIDOCTOOL_PATH")
        if not sync("any/server-external", optional=True):
            sync("any/server-external-" + release_version)

        if box in ("tx1", "edge1"):
            sync("openldap")
            sync("sasl2")

    if platform in ("linux", "windows"):
        sync("%s/doxygen" % platform, path_variable="doxygen_directory")

    if box == "bpi":
        # Lite Client dependencies.
        #sync("fontconfig-2.11.0")
        #sync("additional-fonts")
        #sync("read-edid-3.0.2")
        #sync("a10-display")

        # Hardware video decoding in Lite Client on Debian 7; kernel upgrade.
        #sync("libvdpau-sunxi-1.0-deb7")
        #sync("ldpreloadhook-1.0-deb7")
        #sync("libpixman-0.34.0-deb7")
        #sync("libcedrus-1.0-deb7")
        #sync("uboot-2014.04-10733-gbb5691c-dirty-vanilla")

        # Required to build Lite Client with proxy-decoder support.
        sync("proxy-decoder-deb7")

        # Required for ffmpeg.
        sync("libvdpau-1.0.4.1")

    sync("any/certificates", path_variable="certificates_path")
    sync("any/root-certificates", path_variable="root_certificates_path")


def parse_target(target):
    components = target.split("-")

    platform = None
    arch = None
    box = "none"

    if len(components) > 1:
        platform, arch = components

        if platform == "android":
            box = platform
    else:
        box = target

        if box == "ios":
            platform = "ios"
        else:
            platform = "linux"

            if box == "tx1":
                arch = "aarch64"
            else:
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

    args = parser.parse_args()

    platform, arch, box = parse_target(args.target)

    version_overrides, location_overrides = parse_overrides(args.overrides)
    options = parse_options(args.options)

    syncher = RdepSyncher(args.packages_dir)
    if args.target == "bananapi":
        syncher.rdep_target = "bpi"
    else:
        syncher.rdep_target = args.target
    syncher.versions = determine_package_versions(
        platform,
        box,
        args.release_version,
        customization=args.customization,
        debug=args.debug
    )
    syncher.versions.update(version_overrides)
    syncher.locations = location_overrides
    syncher.use_local = args.use_local

    sync_dependencies(syncher, platform, arch, box, args.release_version, options)

    syncher.generate_cmake_include(args.cmake_include_file)


if __name__ == "__main__":
    main()
