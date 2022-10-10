#!/usr/bin/env python

## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

from conans import ConanFile, __version__ as conan_version
from conans.model import Generator
from conans.tools import Version
from itertools import chain
from pathlib import Path


required_conan_version = ">=1.38.0"


# Conan does not provide a generator which makes it possible to easily access package folders for
# packages in both host and build contexts. This class is the replacement.
class BaseConanPackagePathsGenerator(Generator):
    @property
    def filename(self):
        return "conan_paths.cmake"

    @property
    def content(self):
        def toPosixPath(path):
            return path.replace("\\", "/")

        packages = {reference.ref.name: toPosixPath(package.package_folder) \
            for reference, package in chain(
                self.conanfile.dependencies.build.items(),
                self.conanfile.dependencies.host.items())}

        content = "set(CONAN_DEPENDENCIES {})\n\n".format(" ".join(name for name in packages.keys()))

        for name, path in packages.items():
            content += f"set(CONAN_{name.upper()}_ROOT \"{path}\")\n"

        return content


# Workaround to use the same generator name in the NxOpenConan subclass.
class ConanPackagePathsGenerator(BaseConanPackagePathsGenerator):
    pass


class NxOpenConan(ConanFile):
    settings = "os", "arch", "compiler", "build_type"

    generators = "cmake_find_package", "virtualrunenv", "ConanPackagePathsGenerator"

    options = {
        "targetDevice": "ANY",
        "useClang": (True, False),
    }
    default_options = {
        "targetDevice": None,
        "useClang": False,
    }

    ffmpeg_version = "4.4"
    requires = (
        f"ffmpeg/{ffmpeg_version}" "#bf41f78686975b86ad91daab7f199c4c",
        "openssl/1.1.1k" "#671787bc7a7b737a22baa973d4b25df7",
        "qt/5.15.2" "#86503340155e8896b26aa6ce2e1905fe",
        "roboto-fonts/1.0" "#d9dbbcbc40cbdeb6dfaa382399bccfd6",
    )

    def build_requirements(self):
        if self.isLinux:
            # Note: For gcc-toolchain requirement see open/cmake/conan_profiles/gcc.profile.
            if self.options.useClang:
                self.build_requires("clang/11.0.1" "#f9fc45fe77910654c04a3b7517784fc7")
            self.build_requires("sdk-gcc/5.5" "#53c2ddb9615885ac85d38be2ec272d8e")

        if self.isWindows:
            self.build_requires("wix/3.11" "#fdab0a403cf050e75921ecdb6fa90690")

        if self.haveDesktopClient or self.haveMediaserver:
            self.build_requires("doxygen/1.8.14" "#100a69d210981b259d85d575da0e5e7d")

    def requirements(self):
        self.requires("boost/1.76.0" "#bf922d85865a39cc5d2bc9dc3cf3ff6e")

        # Until we have arm64 macs in CI to build native tools, run x86_64 tools using Rosetta 2.
        if self.isMacos and self.settings.arch in ("armv8", "x86_64"):
            self.options["qt"].tools_target = "Macos_x86_64"

        if self.isLinux:
            if self.settings.arch == "x86_64":
                self.requires("libva/2.6" "#868f4ccc7d32875978f3fae99b79eb07")
                self.requires("intel-media-sdk/19.4" "#c30f624d2a10677b50cb70d02897812b")

            if not self.isArm32:
                self.requires("os_deps_for_desktop_linux/ubuntu_xenial"
                    "#b4e7ff961f0fb8957c5d5a755d5eb55a")

        if self.haveDesktopClient:
            self.requires("hidapi/0.10.1" "#badb3592592c0b6009d844e78060bd41")

            if not self.isEdge1:
                self.requires("flite/2.2" "#89ef970a23fed33fb5f8785e073ab5e1")

        if self.isWindows or self.isAndroid or self.isNx1:
            self.requires("openal/1.19.1" "#eead8dabd1aaed0d105ab3ae8be6473f")

        if self.isWindows:
            self.requires("directx/JUN2010" "#3272f55697228596a7708784a0473927")
            self.requires("intel-media-sdk-bin/2019R1" "#d82fd5a48e99fdea7334167360e463f5")
            self.requires("msvc-redist/2019" "#b25579ed41662d692417dc4b8f59069d")
            self.requires("winsdk-redist/10.0.19041.0" "#422b9a7a082bbb4a2c1326cecbf75aea")

        if self.isArm32 or self.isArm64:
            self.requires("sse2neon/efcbd5" "#3afbbf213755a8fbd8a00e71c2dbc228")

        self._add_documentation_requirements()

    def _add_documentation_requirements(self):
        self.requires(f"vms_help/5.0_patch" "#8ef6477cf684f14f582906a5633460ad")
        self.requires(f"vms_quick_start_guide/5.0" "#568c88a5b8ef893dd3735a44e2c01215")

    def imports(self):
        if self.isLinux:
            self.copy("*.so*", "lib", "lib", root_package="qt")
            self.copy("QtWebEngineProcess", "libexec", "libexec", root_package="qt")

        copy_packages = [
            "openssl",
            "hidapi",
        ]

        if self.isLinux:
            if self.settings.arch == "x86_64":
                copy_packages.append("libva")
                copy_packages.append("intel-media-sdk")

            if not self.isArm32:
                copy_packages.append("ffmpeg")
        else:
            if self.isWindows:
                copy_packages.append("intel-media-sdk-bin")
                copy_packages.append("directx/JUN2010")

            copy_packages.append("ffmpeg")

        if self.isWindows or self.isAndroid or self.isNx1:
            copy_packages.append("openal")

        self._copy_packages(copy_packages)

        self.copy("*.ttf", "bin", "bin", root_package="roboto-fonts")
        if "vms_help" in list(self.requires):
            self.copy("*", "bin", "bin", root_package="vms_help")

    def _copy_packages(self, packages):
        for package in packages:
            if package in self.deps_cpp_info.deps:
                if self.isLinux or self.isAndroid:
                    self.copy("*.so*", "lib", "lib", root_package=package)
                if self.isMacos:
                    self.copy("*.dylib", "lib", "lib", root_package=package)
                if self.isWindows:
                    self.copy("*.dll", "bin", "bin", root_package=package)
                    self.copy("*.pdb", "bin", "bin", root_package=package)

        self.fixLibraryPermissions()

    # Temporary workaround for conan issue 6470.
    def fixLibraryPermissions(self):
        if self.isWindows:
            return

        if self._lib_path.is_dir():
            self.run(f"chmod -R u+w {self._lib_path}")

    @property
    def isLinux(self):
        return self.settings.os == "Linux"

    @property
    def isWindows(self):
        return self.settings.os == "Windows"

    @property
    def isMacos(self):
        return self.settings.os == "Macos"

    @property
    def isAndroid(self):
        return self.settings.os == "Android"

    @property
    def isLinux(self):
        return self.settings.os == "Linux"

    @property
    def isArm32(self):
        return self.settings.arch in ("armv7", "armv7hf")

    @property
    def isArm64(self):
        return self.settings.arch == "armv8"

    @property
    def isEdge1(self):
        return self.options.targetDevice == "edge1"

    @property
    def isNx1(self):
        return self.options.targetDevice == "bpi"

    @property
    def isIos(self):
        return self.settings.os == "iOS"

    @property
    def haveMediaserver(self):
        return not (self.isAndroid or self.isIos)

    @property
    def haveDesktopClient(self):
        return self.isWindows or self.isMacos \
               or (self.isLinux and (self.settings.arch in ("x86_64", "armv8")))

    @property
    def haveMobileClient(self):
        return self.haveDesktopClient or self.isAndroid or self.isIos

    @property
    def _lib_path(self):
        return Path(self.install_folder) / "lib"
