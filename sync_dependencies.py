#!/usr/bin/env python


from __future__ import print_function
from rdep import Rdep
from rdep_cmake import RdepSyncher
from vms_configuration import *


def determine_package_versions():
    v = {
        "qt": "5.6.2",
        "boost": "1.66.0",
        "openssl": "1.0.2e",
        "ffmpeg": "3.1.1",
        "quazip": "0.7.1",
        "sigar": "1.7",
        "openldap": "2.4.42",
        "sasl2": "2.1.26",
        "openal": "1.16",
        "libjpeg-turbo": "1.4.2",
        "festival": "2.4",
        "directx": "JUN2010",
        "pandoc": "2.2.1",
        "detours": "4.0.1",
        "stackwalker": "1.0"
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

    if platform == "macosx":
        v["qt"] = "5.6.3"
        v["ffmpeg"] = "3.1.9"
        v["openssl"] = "1.0.2e-2"
        v["quazip"] = "0.7.3"
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
        v["quazip"] = "0.7"
        v["festival"] = "2.4-1"
        v["festival-vox"] = "2.4"
        v["sysroot"] = "1"

    if box == "bananapi":
        v["ffmpeg"] = "3.1.1-bananapi"
        v["qt"] = "5.6.3-bananapi"

    if box == "rpi":
        v["qt"] = "5.6.3"
        v["quazip"] = "0.7.2"
        v["festival"] = "2.4-1"
        v["festival-vox"] = "2.4"

    if box == "edge1":
        v["qt"] = "5.6.3"
        v["openssl"] = "1.0.1f"
        v["quazip"] = "0.7.2"

    if box == "tx1":
        v["festival"] = "2.1x"
        v["openssl"] = "1.0.0j"
        v["quazip"] = "0.7"

    if not "festival-vox" in v:
        v["festival-vox"] = v["festival"]

    return v


def sync_dependencies(syncher):
    def sync(package, **kwargs):
        return syncher.sync(package, use_local=not rdepSync, **kwargs)

    sync("qt", path_variable="QT_DIR")
    sync("any/boost")

    sync("any/nx_kit")
    sync("any/detection_plugin_interface")

    if box in ("rpi", "bpi", "bananapi"):
        sync("linux-arm/openssl")
    else:
        sync("openssl")

    sync("ffmpeg")

    if platform == "linux" and box in ("bpi", "bananapi", "rpi", "tx1", "none"):
        sync("sysroot", path_variable="sysroot_directory")

    if box == "bpi":
        sync("opengl-es-mali")

    if box == "rpi":
        sync("cifs-utils")

    if platform in ("android", "windows") or box == "bpi":
        sync("openal")

    if not platform in ("android", "ios"):
        sync("quazip")

    if platform == "linux" and box == "none":
        sync("cifs-utils")
        sync("appserver-2.2.1")

    if platform == "windows":
        sync("directx")
        sync("vcredist-2015", path_variable="vcredist_directory")
        sync("vmaxproxy-2.1")
        sync("windows/wix-3.11", path_variable="wix_directory")
        sync("windows/ilmerge", path_variable="ilmerge_directory")

    if platform in ("windows", "linux"):
        sync("%s/pandoc" % platform, path_variable="pandoc_directory")

    if box == "edge1":
        sync("cpro-1.0.1")
        sync("gdb")

    if withDesktopClient:
        sync("any/help-{}-3.2".format(customization), path_variable="help_directory")

    if (platform == "windows") and withDesktopClient:
        sync("detours")
        sync("stackwalker")

    if withDesktopClient or withMobileClient:
        sync("any/roboto-fonts", path_variable="fonts_directory")

    if (withMediaServer or withDesktopClient) and box != "edge1":
        sync("festival")
        if syncher.versions["festival-vox"] != "system":
            sync("any/festival-vox", path_variable="festival_vox_directory")

    if platform in ("android", "ios"):
        sync("libjpeg-turbo")

    if withMediaServer:
        sync("any/nx_sdk-1.7.1")
        sync("any/nx_storage_sdk-1.7.1")
        sync("sigar")

        sync("any/apidoctool-2.0", path_variable="APIDOCTOOL_PATH")

        if customWebAdminPackageDirectory:
            pass
        elif customWebAdminVersion:
            sync("any/server-external-" + customWebAdminVersion)
        else:
            if not sync("any/server-external-" + branch, optional=True):
                sync("any/server-external-" + releaseVersion)

        if box in ("tx1", "edge1"):
            sync("openldap")
            sync("sasl2")

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

    sync("any/certificates-" + customization, path_variable="certificates_path")


def main():
    syncher = RdepSyncher(PACKAGES_DIR)
    syncher.versions = determine_package_versions()
    syncher.rdep_target = rdep_target
    syncher.prefer_debug_packages = prefer_debug_packages

    sync_dependencies(syncher)

    syncher.generate_cmake_include(cmake_include_file)


if __name__ == "__main__":
    main()
