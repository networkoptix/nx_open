#!/usr/bin/env python

## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

from conan.errors import ConanException
from conan.tools.build import cross_building
from conan.tools.files import save
from conan.tools.system.package_manager import Apt
from conans import ConanFile, __version__ as conan_version
from conans.model import Generator
from conans.tools import Version
from itertools import chain
from pathlib import Path
import sys
import yaml


required_conan_version = ">=1.53.0"


# Conan does not provide a generator which makes it possible to easily access package folders for
# packages in both host and build contexts. This is the replacement.
def generate_conan_package_paths(conanfile):
    def toPosixPath(path):
        return path.replace("\\", "/")

    packages = {reference.ref.name: toPosixPath(package.package_folder) \
    # TODO: #dklychkov Investigate whether this is required.
        for reference, package in chain(
            conanfile.dependencies.build.items(),
            conanfile.dependencies.host.items())}

    content = "set(CONAN_DEPENDENCIES {})\n\n".format(" ".join(name for name in packages.keys()))

    for name, path in packages.items():
        content += f"set(CONAN_{name.upper()}_ROOT \"{path}\")\n"

    save(conanfile, "conan_paths.cmake", content)


def generate_conan_package_refs(conanfile):
    refs = [package.pref.full_str() \
        for _, package in chain(
                conanfile.dependencies.build.items(),
                conanfile.dependencies.host.items())]

    save(conanfile, "conan_refs.txt", "\n".join(refs))


class NxOpenConan(ConanFile):
    settings = "os", "arch", "compiler", "build_type"

    generators = "cmake_find_package", "virtualrunenv"

    options = {
        "targetDevice": "ANY",
        "useClang": (True, False),
        "skipCustomizationPackage": (True, False),
        "customization": "ANY",
        "installRuntimeDependencies": (True, False),
    }
    default_options = {
        "targetDevice": None,
        "useClang": False,
        "skipCustomizationPackage": False,
        "customization": "default",
        "installRuntimeDependencies": True,
        "quick_start_guide:format": "pdf",
    }

    ffmpeg_version_and_revision = "4.4#1edf014620e091c2d51dca81685f092b"
    requires = (
        f"ffmpeg/{ffmpeg_version_and_revision}",
        "openssl/1.1.1q" "#a23bd98469b500b2d658a17351fa279c",
        "qt/6.5.3" "#11dd688b8a075726c3bf92a1c67c3d33",
        "roboto-fonts/1.0" "#a1d64ec2d6a2e16f8f476b2b47162123",
        "vms_help/6.0.0",
        "quick_start_guide/6.0.0",
    )

    def configure(self):
        # The open-source Customization Package coming from Conan has the name "opensource-meta",
        # but its id (the "id" field in description.json inside the zip) is "metavms".
        self.options["customization"].customization = "opensource-meta"
        self.options["vms_help"].customization = "metavms"
        self.options["quick_start_guide"].customization = "metavms"

    def generate(self):
        generate_conan_package_paths(self)
        generate_conan_package_refs(self)

    @staticmethod
    def read_deb_dependencies(dependencies_list_file):
        with open(dependencies_list_file, "r") as file:
            data = yaml.load(file, yaml.Loader)

        def trim_version(dep_str):
            return dep_str.split(" ", 1)[0]

        def enumerate_deps(deps):
            for dep in deps:
                if isinstance(dep, str):
                    yield trim_version(dep)
                elif isinstance(dep, list):
                    yield trim_version(dep[0])

        yield from enumerate_deps(data.get("depends", []))
        yield from enumerate_deps(data.get("recommends", []))

    def install_system_requirements(self, packages):
        if cross_building(self):
            # System requirements installation does not work properly in Conan 1.x for cross
            # builds.
            self.output.warn("-DinstallSystemRequirements=ON is ignored for cross builds.")
            return

        try:
            Apt(self).install(packages, check=True)
        except ConanException as e:
            self.output.error(e)
            self.output.error("To install the missing system requirements automatically, "
                "invoke CMake with -DinstallSystemRequirements=ON")
            exit(1)

    def system_requirements(self):
        if sys.platform != "linux":
            return

        packages = [
            "chrpath",
            "pkg-config",
        ]

        if self.options.installRuntimeDependencies:
            src_dir = Path(__file__).parent

            packages += list(NxOpenConan.read_deb_dependencies(
                f"{src_dir}/vms/distribution/deb/client/deb_dependencies.yaml"))

        self.install_system_requirements(packages)

    def build_requirements(self):
        self.build_requires("qt-host/6.5.3" "#eb144d410dfc2e7ee3986101cd3c0fc7")

        if self.isLinux:
            # Note: For gcc-toolchain requirement see open/cmake/conan_profiles/gcc.profile.
            if self.options.useClang:
                self.build_requires("clang/15.0.2" "#9bac3eb3c13d8d3aace16ac4b5fce8d2")
            self.build_requires("sdk-gcc/7.5" "#8e4d5d65d516a14449a95f3c314189f8")

        if self.isWindows:
            self.build_requires("wix/3.11" "#a662d89b677af4a98ac8cd2371be63b8")

        if self.haveDesktopClient or self.haveMediaserver:
            self.build_requires("doxygen/1.8.14" "#ad17490b6013c61d63e10c0e8ea4d6c9")

    def requirements(self):
        if not self.options.skipCustomizationPackage:
            self.requires("customization/1.0")  #< Always use the latest revision.

        self.requires("boost/1.83.0" "#e0be85c6f8107d7e960246e31cbbf7ab")
        self.requires("rapidjson/1.1.0" "#c2e048b9e956f5d9ff93d2d72b4486da")

        if self.isWindows or self.isLinux:
            if self.settings.arch == "x86_64":
                self.requires("cuda-toolkit/11.7" "#f3ed99420b0e949f3187411c368d7aff")
                self.requires("libvpl/2023.4.0" "#5c092e69a8d1ffd163d6db5a3b79c576")
                self.requires("zlib/1.2.12" "#bb959a1d68d4c35d0fba4cc66f5bb25f")

        if self.isLinux:
            if self.settings.arch == "x86_64":
                self.requires("libva/2.16.0" "#a0949cd8f15ba93d12dd835002fe1170")
                self.requires("intel-media-sdk/19.4" "#ecb7939833f8de0ffb197905a4f5a75a")
                self.requires("intel-onevpl/23.4.2" "#da72e6e5e4cb8ce4baf25ce4c0b1a602")

                self.requires("libmysqlclient/8.0.17" "#87d0d0dca416ff91ff910c66b57eab1a")
                self.requires("libpq/13.4" "#3c130555eda25ad50be3824716b0ce4d")

            if not self.isArm32:
                self.requires("os_deps_for_desktop_linux/ubuntu_bionic"
                    "#eb589ca476b9b48622da3261e5342d4d")
                self.requires("legacy_os_deps_from_ubuntu_xenial/1.0"
                    "#665d5fc7371ad819e30bb2d25b529846")

        if self.haveDesktopClient:
            self.requires("hidapi/0.10.1" "#569759f2f39447fe3dbef070243585cc")
            self.requires("pathkit/d776371" "#d1516a12d5e1e70fc8253a501acb3a7f")

            if not self.isEdge1:
                self.requires("flite/2.2" "#069d57cbc32aa09dcbae1c79e94e48ef")
                self.requires("range-v3/0.11.0" "#8d874cb9cdd7b81806d624493b82f9c0")

        if self.isLinux or self.isWindows or self.isAndroid:
            self.requires("openal/1.19.1" "#1047ec92368ace234da430098bffa65a")

        if self.isWindows:
            self.requires("directx/JUN2010" "#ca268f1b54e3874ad43524cd81447b01")
            self.requires("intel-media-sdk-bin/2019R1" "#0a123266dd20b84b16a4cc60b752fc4b")
            self.requires("msvc-redist/14.29.30133" "#9f10aa59e4671ce0669d6181e6b0a269")
            self.requires("winsdk-redist/10.0.20348.0" "#bf8bf6131653c35ae84474f399fe5113")
            self.requires("icu-win/70.1" "#1c50009a7165b74563ba149c315737f1")

        if self.isArm32 or self.isArm64:
            self.requires("sse2neon/7bd15ea" "#d5c087ce33dbf1425b29d6435284d2c7")

    def imports(self):
        if self.isLinux:
            self.copy("*.so*", "lib", "lib", root_package="qt")
            self.copy("QtWebEngineProcess", "libexec", "libexec", root_package="qt")

        if self.isLinux and self.settings.arch == "x86_64":
            self.copy("*.so*", "lib", "lib64/", root_package="cuda-toolkit")

        copy_packages = [
            "openssl",
            "hidapi",
            "cuda-toolkit"
        ]

        if self.isLinux:
            if self.settings.arch == "x86_64":
                copy_packages.append("libva")
                copy_packages.append("intel-media-sdk")
                copy_packages.append("intel-onevpl")
                copy_packages.append("libvpl")

            if not self.isArm32:
                copy_packages.append("ffmpeg")
        else:
            if self.isWindows:
                copy_packages.append("intel-media-sdk-bin")
                copy_packages.append("libvpl")
                copy_packages.append("directx/JUN2010")

            copy_packages.append("ffmpeg")

        if self.isLinux or self.isWindows or self.isAndroid:
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
        return self.isAndroid or self.isIos

    @property
    def haveAnalyticsServer(self):
        return self.isLinux and self.settings.arch == "x86_64"

    @property
    def _lib_path(self):
        return Path(self.install_folder) / "lib"
