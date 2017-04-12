#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""
List of artifacts that must be published
See: https://networkoptix.atlassian.net/wiki/display/SD/Installer+Filenames
"""

class AppType():
    CLIENT = "client"
    SERVER = "server"
    BUNDLE = "bundle"
    SERVERTOOL = "servertool"

    CLIENT_UPDATE = "client_update"
    SERVER_UPDATE = "server_update"

    PAXTON = "paxton"

    CLIENT_DEBUG = "client_debug"
    SERVER_DEBUG = "server_debug"
    CLOUD_DEBUG = "cloud_debug"
    LIBS_DEBUG = "libs_debug"
    MISC_DEBUG = "misc_debug"

class Extension():
    EXE = "exe"
    MSI = "msi"
    ZIP = "zip"
    TGZ = "tar.gz"
    DEB = "deb"
    APK = "apk"
    IPA = "ipa"
    DMG = "dmg"

def winAppTypes():
    return {
        AppType.CLIENT: Extension.EXE,
        AppType.SERVER: Extension.EXE,
        AppType.BUNDLE: Extension.EXE,
        AppType.SERVERTOOL: Extension.EXE,
        AppType.CLIENT_UPDATE: Extension.ZIP,
        AppType.SERVER_UPDATE: Extension.ZIP,
        AppType.CLIENT_DEBUG: Extension.ZIP,
        AppType.SERVER_DEBUG: Extension.ZIP,
        AppType.CLOUD_DEBUG: Extension.ZIP,
        AppType.LIBS_DEBUG: Extension.ZIP,
        AppType.MISC_DEBUG: Extension.ZIP
        }

def linuxAppTypes():
    return {
        AppType.CLIENT: Extension.DEB,
        AppType.SERVER: Extension.DEB,
        AppType.CLIENT_UPDATE: Extension.ZIP,
        AppType.SERVER_UPDATE: Extension.ZIP
        }

def macAppTypes():
    return {
        AppType.CLIENT: Extension.DMG,
        AppType.CLIENT_UPDATE: Extension.ZIP
        }

class Artifact():

    BETA = "beta"

    def __init__(self, product = "", apptype = "", version = "", platform = "", beta = False, cloud = "", extension = ""):
        self.product = product
        self.apptype = apptype
        self.version = version
        self.platform = platform
        self.beta = beta
        self.cloud = cloud
        self.extension = extension

    def parse(self, name):
        # parsing rule {product}-{apptype}-{version}-{platform}-{optional:beta_status}-{optional:cloud_instance_group}.{extension}
        parts = name.split('-')
        last = parts[-1]
        parts.pop()
        last, sep, self.extension = last.partition('.')
        parts.append(last)

        nargs = len(parts)
        assert(4 <= nargs <= 6)

        self.product = parts[0]
        self.apptype = parts[1]
        self.version = parts[2]
        self.platform = parts[3]
        self.beta = False
        self.cloud = ""

        option = parts[4] if nargs > 4 else ""
        self.beta = option == Artifact.BETA
        if nargs == 6:
            assert(self.beta)
            self.cloud = parts[5]
        else:
            self.cloud = option if not self.beta else ""

    def name(self):
        basename = '-'.join([self.product, self.apptype, self.version, self.platform])
        if self.beta:
            basename += '-' + Artifact.BETA
        if self.cloud:
            basename += '-' + self.cloud
        return basename + '.' + self.extension

    def __str__(self):
        return self.name()

    def __repr__(self):
        return "<Artifact {0}>".format(self.name())


def get_artifacts(product, version, beta, cloud):
    for apptype, extension in winAppTypes().items():
        for platform in ["win64", "win86"]:
            yield Artifact(product, apptype, version, platform, beta, cloud, extension)
    yield Artifact(product, AppType.PAXTON, version, platform, beta, cloud, Extension.MSI)
    for apptype, extension in linuxAppTypes().items():
        for platform in ["linux64", "linux86"]:
            yield Artifact(product, apptype, version, platform, beta, cloud, extension)
    for apptype, extension in macAppTypes().items():
        yield Artifact(product, apptype, version, "mac", beta, cloud, extension)
    yield Artifact(product, AppType.CLIENT, version, "android", beta, cloud, Extension.APK)
    yield Artifact(product, AppType.CLIENT, version, "ios", beta, cloud, Extension.IPA)
    for platform in ["bpi", "rpi", "bananapi"]:
        yield Artifact(product, AppType.SERVER, version, platform, beta, cloud, Extension.TGZ)
        yield Artifact(product, AppType.SERVER_UPDATE, version, platform, beta, cloud, Extension.ZIP)

if __name__ == '__main__':
    for a in get_artifacts("hdwitness", "3.0.0.13804", True, "test"):
        print a
