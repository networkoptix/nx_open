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
import shutil
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
        "onlyUnrevisionedPackages": (True, False),
    }
    default_options = {
        "targetDevice": None,
        "useClang": False,
        "skipCustomizationPackage": False,
        "customization": "default",
        "installRuntimeDependencies": True,
        "quick_start_guide:format": "pdf",
        "mobile_user_manual:format": "pdf",
        "onlyUnrevisionedPackages": False,
    }

    ffmpeg_version_and_revision = "4.4#1edf014620e091c2d51dca81685f092b"

    def configure(self):
        # The open-source Customization Package coming from Conan has the name "opensource-meta",
        # but its id (the "id" field in description.json inside the zip) is "metavms".
        self.options["customization"].customization = "opensource-meta"
        self.options["vms_help"].customization = "metavms"
        self.options["quick_start_guide"].customization = "metavms"
        self.options["mobile_user_manual"].customization = "metavms"

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
        self.build_requires("apidoctool/3.0" "#85521f0529e13e8436d41a1974a47e1f")
        self.build_requires("qt-host/6.5.3" "#cb4a89251221cd4683f58f03fd308218")
        self.build_requires("swagger-codegen/3.0.21" "#0811242b97c1aa31a027c236fce5adbd")

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

        if self.haveDesktopClient:
            self.requires("vms_help/6.0.0")
            self.requires("quick_start_guide/6.0.0")
            self.requires("mobile_user_manual/21.2")

        if self.options.onlyUnrevisionedPackages:
            return

        self.requires(f"ffmpeg/{self.ffmpeg_version_and_revision}")
        self.requires("openssl/1.1.1q" "#a23bd98469b500b2d658a17351fa279c")
        self.requires("qt/6.5.3" "#4a20c9e2d211d3aaaf8b04606bbbe643")
        self.requires("roboto-fonts/1.0" "#a1d64ec2d6a2e16f8f476b2b47162123")
        self.requires("boost/1.83.0" "#e0be85c6f8107d7e960246e31cbbf7ab")
        self.requires("rapidjson/cci.20230929" "#624c0094d741e6a3749d2e44d834b96c")

        if self.isWindows or self.isLinux:
            if self.settings.arch == "x86_64":
                self.requires("cuda-toolkit/11.7" "#f3ed99420b0e949f3187411c368d7aff")
                self.requires("libvpl/2023.4.0" "#d3846418a105faa17d740e65a5b98398")
                self.requires("zlib/1.2.12" "#bb959a1d68d4c35d0fba4cc66f5bb25f")

        if self.isLinux:
            if self.settings.arch == "x86_64":
                self.requires("libva/2.16.0" "#a0949cd8f15ba93d12dd835002fe1170")
                self.requires("intel-media-sdk/19.4" "#ecb7939833f8de0ffb197905a4f5a75a")
                self.requires("intel-onevpl/23.4.2" "#da72e6e5e4cb8ce4baf25ce4c0b1a602")

                self.requires("libmysqlclient/8.0.17" "#87d0d0dca416ff91ff910c66b57eab1a")
                self.requires("libpq/13.4" "#3c130555eda25ad50be3824716b0ce4d")

            if not self.isArm32:
                self._os_deps_package = "os_deps_for_desktop_linux"
                self.requires("os_deps_for_desktop_linux/ubuntu_bionic"
                    "#a75c6592816cd2d5a60b73b457eaa6ac")
                self.requires("legacy_os_deps_from_ubuntu_xenial/1.0"
                    "#14bc87c62a0184c7f8c9ddefacf30a83")

        if self.haveDesktopClient:
            self.requires("hidapi/0.10.1" "#569759f2f39447fe3dbef070243585cc")
            self.requires("pathkit/d776371" "#d1516a12d5e1e70fc8253a501acb3a7f")

            if not self.isEdge1:
                self.requires("flite/2.2" "#069d57cbc32aa09dcbae1c79e94e48ef")
                self.requires("range-v3/0.11.0" "#8d874cb9cdd7b81806d624493b82f9c0")

        if self.isLinux or self.isAndroid:
            self.requires("openal/1.19.1" "#1047ec92368ace234da430098bffa65a")
        elif self.isWindows:
            self.requires("openal/1.23.1-ec2ffb" "#cefaa7b20e912b242b87f5c212427d81")

        if self.isWindows:
            self.requires("directx/JUN2010" "#ca268f1b54e3874ad43524cd81447b01")
            self.requires("intel-media-sdk-bin/2019R1" "#0a123266dd20b84b16a4cc60b752fc4b")
            self.requires("msvc-redist/14.38.33135" "#716dc6c575a38ac17943d6f0f45dde6d")
            self.requires("winsdk-redist/10.0.22621.0" "#8dfc5bcbabe971a46f78c8d2657d7ec2")
            self.requires("icu-win/70.1" "#1c50009a7165b74563ba149c315737f1")

        if self.isArm32 or self.isArm64:
            self.requires("sse2neon/7bd15ea" "#d5c087ce33dbf1425b29d6435284d2c7")

    def prepare_pkg_config_files(self):
        if self.isLinux:
            pc_files_dir = Path(self.build_folder) / "os_deps_pkg_config"
            script = self.deps_user_info[self._os_deps_package].pkg_config_transformer_script
            self.run(f"{script} -o {pc_files_dir}")

    def import_files_from_package(self, package, src, dst, glob):
        if package not in self.deps_cpp_info.deps:
            return

        src_dir = Path(self.deps_cpp_info[package].rootpath) / src
        dst_dir = Path(self.install_folder) / dst

        for file in src_dir.glob(glob):
            if not file.is_dir():
                dst_file = dst_dir / file.relative_to(src_dir)
                self.__imported_files[dst_file] = file

    def import_package(self, package):
        if self.isLinux or self.isAndroid:
            self.import_files_from_package(package, "lib", "lib", "*.so*")
        if self.isMacos:
            self.import_files_from_package(package, "lib", "lib", "*.dylib")
        if self.isWindows:
            self.import_files_from_package(package, "bin", "bin", "*.dll")
            self.import_files_from_package(package, "bin", "bin", "*.pdb")

    def finish_import(self):
        for dst, src in self.__imported_files.items():
            if dst.exists() and dst.stat().st_mtime >= src.stat().st_mtime:
                continue

            if not dst.parent.exists():
                dst.parent.mkdir(parents=True)

            dst.unlink(missing_ok=True)
            shutil.copyfile(src, dst, follow_symlinks=False)

        self.fixLibraryPermissions()

        with open(Path(self.install_folder) / "conan_imported_files.txt", "w") as file:
            for dst, src in self.__imported_files.items():
                file.write(f"{dst}: {src}\n")

    def imports(self, finish_import=True):
        self.prepare_pkg_config_files()

        self.__imported_files = {}

        if self.isLinux:
            self.import_files_from_package("qt", "libexec", "libexec", "QtWebEngineProcess")

        if self.isLinux and self.settings.arch == "x86_64":
            self.import_files_from_package("cuda-toolkit", "lib64", "lib", "*.so*")

        self.import_files_from_package("roboto-fonts", "bin/fonts", "bin/fonts", "*.ttf")

        if "vms_help" in list(self.requires):
            self.import_files_from_package("vms_help", "help", "bin/help", "**/*")

        self.import_package("openssl")
        self.import_package("hidapi")
        self.import_package("cuda-toolkit")

        if self.isLinux:
            self.import_package("qt")
            if self.settings.arch == "x86_64":
                self.import_package("libva")
                self.import_package("intel-media-sdk")
                self.import_package("intel-onevpl")
                self.import_package("libvpl")

            if not self.isArm32:
                self.import_package("ffmpeg")
        else:
            if self.isWindows:
                self.import_package("intel-media-sdk-bin")
                self.import_package("libvpl")
                self.import_package("directx/JUN2010")

            self.import_package("ffmpeg")

        if self.isLinux or self.isWindows or self.isAndroid:
            self.import_package("openal")

        if finish_import:
            self.finish_import()

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
