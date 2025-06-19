## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

from conan.errors import ConanException
from conan.tools.build import cross_building
from conan.tools.files import save
from conan.tools.system.package_manager import Apt
from conan import ConanFile
from itertools import chain
from pathlib import Path
import platform
import shutil
import sys
import yaml
import os


required_conan_version = ">=1.53.0"


# Help packages are not required to be built from the same commit.
VMS_HELP_VERSION = "6.1.0-77eebbb9ab9ebbe0d84ff125f6975460b35db147"
QUICK_START_GUIDE_VERSION = "6.0.0-bfdf1ce0a6f533e97993e3b2e088696549d94f62"
MOBILE_USER_MANUAL_VERSION = "25.1-77eebbb9ab9ebbe0d84ff125f6975460b35db147"


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
        "customization": "ANY",
        "installRuntimeDependencies": (True, False),
        "withAnalyticsServer": (True, False),
    }
    default_options = {
        "targetDevice": None,
        "useClang": False,
        "customization": "metavms",
        "installRuntimeDependencies": True,
        "withAnalyticsServer": True,
        "quick_start_guide:format": "pdf",
        "mobile_user_manual:format": "pdf",
    }

    ffmpeg_version_and_revision = "7.0.1#7395197c94840d269665fbcea07ada79"

    python_requires = (
        "os_deps_from_deb_based_distro/0.5" "#43dce86a813993ad9acb644d3941e399",
        "os_deps_activator/0.1" "#f401fca7dbd657d5be3c91e621ed94a3",
    )

    def configure(self):
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
                    yield [trim_version(alternative) for alternative in dep]

        yield from enumerate_deps(data.get("depends", []))
        yield from enumerate_deps(data.get("recommends", []))

    def install_system_requirements(self, packages):
        if cross_building(self):
            # System requirements installation does not work properly in Conan 1.x for cross
            # builds.
            self.output.warn("-DinstallSystemRequirements=ON is ignored for cross builds.")
            return

        plain_deps = [dep for dep in packages if isinstance(dep, str)]
        alternating_deps = [dep for dep in packages if isinstance(dep, list)]

        try:
            Apt(self).install(plain_deps, check=True)
            for alternatives in alternating_deps:
                Apt(self).install_substitutes(check=True, *[[dep] for dep in alternatives])
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
        self.build_requires("apidoctool/3.0" "#483c5073667ee722756e0ca31e18972a")
        self.build_requires("qt-host/6.9.0" "#3995869eb604dd7c10d91e6869ca2a9f")
        self.build_requires("swagger-codegen/3.0.21" "#82967d62d27833281bd87e044d6e50f9")

        if self.isLinux:
            # Note: For gcc-toolchain requirement see open/cmake/conan_profiles/gcc.profile.
            if self.options.useClang:
                self.build_requires("clang/20.1.2" "#45a020e716ad31135c131b24734f0769")
            self.build_requires("sdk-gcc/9.5" "#4934f8197fb3e1d7812bd951b1cbae85")

        if self.isWindows:
            self.build_requires("wix/3.14.1" "#51b4f9f9375a35447d8b88b5e832f831")

        if self.haveDesktopClient or self.haveMediaserver:
            self.build_requires("doxygen/1.8.14" "#e4d349d41cd2ea37812b3f284bd88784")

        if self.isAndroid:
            self.build_requires("openjdk/18.0.1" "#a8a02e50d3ff18df2248cae06ed5a13c")
            if "ANDROID_HOME" not in os.environ:
                self.build_requires("AndroidSDK/34" "#eea6103b2dcc6cd808d0e8c2ee512bf9")
            if "ANDROID_NDK" not in os.environ:
                self.build_requires("AndroidNDK/r26d" "#0ae8a952a8b231f98f2f7f2d61fd249a")
        else:
            # Java runtime for apidoctool.
            self.tool_requires("openjdk-jre/17.0.12" "#ceed4d8b4fdfbd3f680688f67488dc27")

    def requirements(self):
        self.requires("cpptrace/0.8.3" "#336ded531d0cad8ec579eb05079591e0")

        self.requires("opentelemetry-cpp/1.17.0" "#feb089e3fbaef23a752681d2b96379bd")
        # OpenTelemetry dependencies.
        self.requires("c-ares/1.34.3" "#1f1b2f929424608c837837ea6379ae15")
        self.requires("protobuf/5.27.0" "#56d591557e0fc6a4356fc1dbc6ffbe56")
        self.requires("grpc/1.67.1" "#af343deb43728d9f31d2a7c9fc0728f5")
        self.requires("abseil/20240116.2" "#129b9a5c87da55d656811cb53e915b41")
        self.requires("re2/20230301" "#5504bfc6731b5c7a12ff524a6b2205c1")
        self.requires("opentelemetry-proto/1.3.2" "#14665af6359f2a239e81925285e5b654")

        self.requires("libsrtp/2.6.0" "#248ee72d7d91db948f5651b7fe4905ea")
        self.requires(f"ffmpeg/{self.ffmpeg_version_and_revision}")
        self.requires("openssl/1.1.1q" "#cf9c0c761f39805e5a258dc39daff2bd")
        self.requires("qt/6.9.0" "#bde40d4b323b30ce62c888d3f114dc40")
        self.requires("roboto-fonts/1.0" "#1bff09c31c4d334f27795653e0f4b2bb")
        self.requires("boost/1.83.0" "#d150c9edc8081c98965b05ea9c2df318")
        self.requires("rapidjson/cci.20230929" "#751fc0dfc70af706c708706450fc2ab7")
        self.requires("zlib/1.3.1" "#99d6f9ea0a1dd63d973392c24ce0aa9b")
        self.requires("perfetto/47.0" "#fefcb910df242e7dca2a309cac9396cb")

        if self.settings.os not in ("Android", "iOS"):
            # Qt dependency.
            self.requires("icu/74.2" "#a0ffc2036da25e5dbe72dc941074a6c4")
            # FFmpeg dependencies.
            self.requires("ogg/1.3.5" "#00fb0bd978d034d12af5efd5d6921364")
            self.requires("vorbis/1.3.7" "#0400cbb550b491521361a41c889d5c48")
            self.requires("libmp3lame/3.100" "#da13ecbaf0d06421ae586b7226d985ad")
            self.requires("libvpx/1.14.1" "#fb51b8d71add5751343a0d06ed3bb44e")
            self.requires("openh264/2.4.1" "#e7846aa3511316c230721ca13fc9a127")
            self.requires("freetype/2.12.1" "#cd63b23b3aa630a38fa870a267e95782")

        if self.isWindows or self.isLinux:
            self.requires("vulkan-headers/1.3.290.0" "#6a0d12455e50dca266c79b88fda818b3")
            if self.settings.arch == "x86_64":
                self.requires("cuda-toolkit/12.5.1" "#895935cfab790040df87c288d2873f04")
                self.requires("libvpl/2023.4.0" "#22d0df9697d26ecbb784e71a2c882e05")
                self.requires("libpq/15.5" "#fa107fbe709db74faa6e2cb3cf18a5ae")

        if self.isLinux:
            if self.settings.arch == "x86_64":
                self.requires("libva/2.22.0" "#c3156ed8aeb0461f978b086681a2aa18")
                self.requires("intel-media-sdk/19.4" "#a3645f9972ae962e6ec4f426831bffea")
                self.requires("intel-onevpl/23.4.2" "#f6cce96833acd40bc8e2f0d1759650df")
                self.requires("intel-gmmlib/22.5.2" "#a9a4be5e7f657758b6300e3b09074628")
                self.requires("intel-media-driver/23.4.3" "#9f52c4393479e16d22aaa6c6b57ecf99")
                self.requires("nv-codec-headers/12.1.14.0" "#65e2d80efd67e46fc41f135f2468e3df")

                self.requires("libmysqlclient/8.1.0" "#e762100664bad1c018ad71ecf702ea5e")

            if not self.isArm32:
                self._os_deps_package = "os_deps_for_desktop_linux"
                self.requires("os_deps_for_desktop_linux/ubuntu_focal"
                    "#e89b9e29cdcfa7699fa4810eae291feb")

        if self.haveDesktopClient:
            if self.isMacos:
                self.requires("hidapi/0.14.0" "#67b81bb0b1ef84cedc49b2e3671a48e6")
            self.requires("pathkit/m134" "#0d764b6df74bb96dbd27b40e38bd7f0a")

        if self.isWindows or self.isAndroid or (self.isLinux and not self.isArm32):
            self.requires("openal/ec2ffbfa" "#7a3cdb640bc7c4409c25e55161c30d6a")

        if self.isWindows:
            self.requires("directx/june2010" "#a0cbbd6a9cfef629fae6d2cf5a18bcdd")
            self.requires("intel-media-sdk-bin/2019r1-1" "#40cd824192475ce059cb9529a06a3e13")
            self.requires("msvc-redist/14.38.33135" "#716dc6c575a38ac17943d6f0f45dde6d")
            self.requires("winsdk-redist/10.0.22621.0" "#8dfc5bcbabe971a46f78c8d2657d7ec2")

        if self.isArm32 or self.isArm64:
            self.requires("sse2neon/1.7.0" "#188c824e380c1a5ad666d1229226b7c8")

        if self.settings.os in ("Android", "iOS") or self.haveAnalyticsServer:
            self.requires("libjpeg-turbo/3.0.3" "#1e534ce92aac40555ae9fd1184428b04")

        if self.haveDesktopClient or self.haveMediaserver:
            self.requires("flite/2.2" "#52b50c815dd81e40a21aaabb87d38b50")

        if self.haveDesktopClient:
            self.requires("vms_help/" + VMS_HELP_VERSION)
            self.requires("quick_start_guide/" + QUICK_START_GUIDE_VERSION)
            self.requires("mobile_user_manual/" + MOBILE_USER_MANUAL_VERSION)

    def prepare_pkg_config_files(self):
        if self.isLinux:
            pc_files_dir = Path(self.install_folder) / "os_deps_pkg_config"
            self.python_requires["os_deps_activator"].module.activate_pkg_config(
                self, pc_files_dir=pc_files_dir)

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
            if dst.exists() and dst.stat().st_mtime == src.stat().st_mtime:
                continue

            if not dst.parent.exists():
                dst.parent.mkdir(parents=True)

            dst.unlink(missing_ok=True)
            self.output.info(f"Importing {src} -> {dst}")
            shutil.copy2(src, dst, follow_symlinks=False)

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

        self.import_files_from_package("roboto-fonts", ".", "bin/fonts", "*.ttf")

        if "vms_help" in list(self.requires):
            self.import_files_from_package("vms_help", "help", "bin/help", "**/*")

        self.import_package("openssl")
        self.import_package("icu")
        self.import_package("cuda-toolkit")

        if self.isLinux:
            self.import_package("qt")
            if self.settings.arch == "x86_64":
                self.import_package("libva")
                self.import_package("intel-media-sdk")
                self.import_package("intel-onevpl")
                self.import_package("intel-gmmlib")
                self.import_files_from_package(
                    "intel-media-driver",
                    "lib/dri",
                    "lib/libva-drivers",
                    "*.so*")
                self.import_package("libvpl")

            if not self.isArm32:
                self.import_package("ffmpeg")
        else:
            if self.isWindows:
                self.import_package("intel-media-sdk-bin")
                self.import_package("libvpl")
                self.import_package("directx/JUN2010")
            elif self.isMacos:
                self.import_package("hidapi")

            self.import_package("ffmpeg")

        if self.isLinux or self.isWindows or self.isAndroid:
            self.import_package("openal")

        if finish_import:
            self.finish_import()

    # Temporary workaround for conan issue 6470.
    def fixLibraryPermissions(self):
        if platform.system() == "Windows":
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
    def isX64(self):
        return self.settings.arch == "x86_64"

    @property
    def isArm32(self):
        return self.settings.arch in ("armv7", "armv7hf")

    @property
    def isArm64(self):
        return self.settings.arch == "armv8"

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
        return self.options.withAnalyticsServer and self.isLinux and self.settings.arch == "x86_64"

    @property
    def _lib_path(self):
        return Path(self.install_folder) / "lib"
