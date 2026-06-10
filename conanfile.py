## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

from conan.errors import ConanException
from conan.tools.build import cross_building
from conan.tools.cmake import CMakeDeps
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
VMS_HELP_VERSION = "6.1.1-ba655f5c497face7eeaa4c4b7024f61c6ece1bb2"
QUICK_START_GUIDE_VERSION = "6.1.1-ba655f5c497face7eeaa4c4b7024f61c6ece1bb2"
MOBILE_USER_MANUAL_VERSION = "25.2-ba655f5c497face7eeaa4c4b7024f61c6ece1bb2"


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

    for reference, package in conanfile.dependencies.build.items():
        name = reference.ref.name
        path = toPosixPath(package.package_folder)
        content += f"set(CONAN_BUILD_{name.upper()}_ROOT \"{path}\")\n"

    save(conanfile, "conan_paths.cmake", content)


def generate_conan_package_refs(conanfile):
    refs = [package.pref.repr_notime() \
        for _, package in chain(
                conanfile.dependencies.build.items(),
                conanfile.dependencies.host.items())]

    save(conanfile, "conan_refs.txt", "\n".join(refs))


class NxOpenConan(ConanFile):
    settings = "os", "arch", "compiler", "build_type"

    generators = "VirtualRunEnv"

    options = {
        "targetDevice": ["ANY"],
        "useClang": [True, False],
        "customization": ["ANY"],
        "installRuntimeDependencies": [True, False],
        "withAnalyticsServer": [True, False],
    }
    default_options = {
        "targetDevice": None,
        "useClang": False,
        "customization": "metavms",
        "installRuntimeDependencies": True,
        "withAnalyticsServer": True,
        "quick_start_guide/*:format": "pdf",
        "mobile_user_manual/*:format": "pdf",
    }

    ffmpeg_version_and_revision = "7.0.1#67a29b60d290a9da6f3b044517beffe1"

    python_requires = (
        "os_deps_from_deb_based_distro/0.5" "#4a34b16b3f888ea7239c055feeabb838",
        "os_deps_activator/0.1" "#02d7ab4ef4de01a22b7b4f36acb13361",
    )

    def configure(self):
        self.options["vms_help"].customization = self.options.customization
        self.options["quick_start_guide"].customization = self.options.customization
        self.options["mobile_user_manual"].customization = self.options.customization

    def generate(self):
        deps = CMakeDeps(self)
        # Override cmake_file_name for packages whose Conan name doesn't match
        # what the codebase uses in find_package() calls.
        deps.set_property("ffmpeg", "cmake_file_name", "ffmpeg")
        deps.set_property("cpptrace", "cmake_file_name", "CPPTRACE")
        deps.set_property("cpptrace", "cmake_target_name", "CPPTRACE::CPPTRACE")
        deps.set_property("libdatachannel", "cmake_file_name", "libdatachannel")
        deps.set_property("rapidjson", "cmake_target_name", "RapidJSON::RapidJSON")
        deps.set_property("jinja2cpp", "cmake_target_name", "jinja2cpp::jinja2cpp")
        deps.set_property("openssl", "cmake_target_name", "OpenSSL::OpenSSL")
        deps.set_property("openssl", "cmake_file_name", "OpenSSL")
        deps.set_property("cudnn", "cmake_target_name", "cuDNN::cuDNN")
        deps.set_property("opencv-static", "cmake_target_name", "opencv-static::opencv-static")
        # Map opencv component names: opencv_X -> opencv-static::X
        for comp in ("imgcodecs", "video", "core", "cudev", "flann", "imgproc", "ml",
                      "dnn", "features2d", "photo", "cudawarping", "calib3d",
                      "videoio", "highgui", "objdetect", "stitching"):
            deps.set_property(f"opencv-static::opencv_{comp}",
                              "cmake_target_name", f"opencv-static::{comp}")
        # Conan packages may be built with a different build_type than the CMake project
        # (e.g. Release on Linux, RelWithDebInfo on Windows/Android). Generate cmake
        # integration for all configurations so that targets are not restricted to a single
        # configuration via generator expressions.
        for config in ("Debug", "Release", "RelWithDebInfo"):
            deps.configuration = config
            deps.generate()

        generate_conan_package_paths(self)
        generate_conan_package_refs(self)

        self.imports()

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
            self.output.warning("-DinstallSystemRequirements=ON is ignored for cross builds.")
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
        self.tool_requires("qt-host/6.9.1" "#bf20d28d23fbad4c7190648fa3dae37b")
        self.tool_requires("protobuf/5.27.0" "#56d591557e0fc6a4356fc1dbc6ffbe56")
        self.tool_requires("grpc/1.67.1" "#af343deb43728d9f31d2a7c9fc0728f5")
        self.tool_requires("abseil/20240116.2" "#129b9a5c87da55d656811cb53e915b41")
        self.tool_requires("openssl/1.1.1q" "#3e617c7677392516b6e617f698692fc4")

        if not self.isEmscripten:
            self.tool_requires("apidoctool/3.0" "#02ae3ddf972d89e3bcff43e0f35926d9")
            self.tool_requires("swagger-codegen/3.0.21" "#82967d62d27833281bd87e044d6e50f9")
            self.tool_requires("breakpad-tools/2024.02.16" "#66de8f604ffe19a09a20f2a587624ef1")

        if self.isLinux or self.isWindows:
            # Note: For gcc-toolchain requirement see open/cmake/conan_profiles/gcc.profile.
            if self.options.useClang:
                self.tool_requires("clang/20.1.2" "#493f55bfbb20874208a25ee845a83c3c")
                self.tool_requires("ninja/1.12.1" "#3755ec3c6188d69458474b5353305265")
            if self.isLinux:
                self.tool_requires("sdk-gcc/9.5" "#54cc3c576f5d9f324caaf23152b4074e")

        if self.isWindows:
            self.tool_requires("wix/3.14.1" "#51b4f9f9375a35447d8b88b5e832f831")

        if self.haveDesktopClient or self.haveMediaserver:
            self.tool_requires("doxygen/1.8.14" "#e4d349d41cd2ea37812b3f284bd88784")

        if self.isAndroid:
            self.tool_requires("openjdk/18.0.1" "#d7cdae72f654bff4968cebe42d6530da")
            if "ANDROID_HOME" not in os.environ:
                self.tool_requires("android-sdk/34" "#e304daf9254e7af886b5e153714c5a79")
            if "ANDROID_NDK" not in os.environ:
                self.tool_requires("android-ndk/r29" "#8b725cb46c0e050cf2e168084e7b99fa")
        elif not self.isEmscripten:
            # Java runtime for apidoctool.
            self.tool_requires("openjdk-jre/17.0.12" "#34638acda4638c46a55a6475de444e7e")

    def requirements(self):
        if not self.isEmscripten:
            self.requires("cpptrace/0.8.3" "#336ded531d0cad8ec579eb05079591e0")

            self.requires("opentelemetry-cpp/1.17.0" "#183f60506ee6fae5b404f14ecd9d52b8")
            # OpenTelemetry dependencies.
            self.requires("c-ares/1.34.3" "#1f1b2f929424608c837837ea6379ae15")
            self.requires("protobuf/5.27.0" "#56d591557e0fc6a4356fc1dbc6ffbe56")
            self.requires("grpc/1.67.1" "#af343deb43728d9f31d2a7c9fc0728f5")
            self.requires("abseil/20240116.2" "#daa8cf35bf7547e3f64ad99a2cbbc02f")
            self.requires("re2/20230301" "#5504bfc6731b5c7a12ff524a6b2205c1")
            self.requires("opentelemetry-proto/1.3.2" "#14665af6359f2a239e81925285e5b654")

        self.requires("boost/1.89.0" "#130a884f1529433238f4f2dc98d94ac8")
        self.requires(f"ffmpeg/{self.ffmpeg_version_and_revision}")
        self.requires("openssl/1.1.1q" "#3e617c7677392516b6e617f698692fc4")
        self.requires("qt/6.9.1" "#c4d4348e1423f513bece44d0df8f7553")
        self.requires("rapidjson/cci.20230929" "#9d79a3f161df66fa32001bb500c0898d")
        self.requires("zlib/1.3.1" "#a5b1285cce3a94ea5d51b5d60c1a1fbe")

        if not self.isEmscripten:
            self.requires("libsrtp/2.6.0" "#248ee72d7d91db948f5651b7fe4905ea")
            self.requires("libmp3lame/3.100" "#da13ecbaf0d06421ae586b7226d985ad")
            self.requires("roboto-fonts/1.0" "#1bff09c31c4d334f27795653e0f4b2bb")
            self.requires("perfetto/47.0" "#fefcb910df242e7dca2a309cac9396cb")
            self.requires("crashpad/cci.20250729" "#b49360b710de1716da8e0b886704adbc")

        if self.settings.os not in ("Android", "iOS", "Emscripten"):
            # Qt dependency.
            self.requires("icu/74.2" "#a0ffc2036da25e5dbe72dc941074a6c4")
            # FFmpeg dependencies.
            self.requires("ogg/1.3.5" "#00fb0bd978d034d12af5efd5d6921364")
            self.requires("vorbis/1.3.7" "#0400cbb550b491521361a41c889d5c48")
            self.requires("libvpx/1.14.1" "#fb51b8d71add5751343a0d06ed3bb44e")
            self.requires("openh264/2.6.0" "#d87623b0e02a383eaf37ad97594ebcc8")
            self.requires("freetype/2.13.2" "#7c23baf248eac45b16b362b8288d11e9")

        if self.isWindows or self.isLinux:
            self.requires("vulkan-headers/1.3.290.0" "#6a0d12455e50dca266c79b88fda818b3")
            if self.settings.arch == "x86_64":
                self.requires("cuda-toolkit/12.5.1" "#d3a6ce515fbfdb0e5487ea4ca7a471de")
                self.requires("libvpl/2023.4.0" "#8bda31d40a7cc00639000d6bd3e4faf9")
                self.requires("libpq/15.5" "#21f4f264e8a3e8627735414ba856999b")

            # Required by the mqtt_plugin (open-source server plugin).
            # On Windows platform some packages from PyPI are not compatible with the self-built
            # CPython, so we use re-packaged Python distribution from the official web site instead.
            if self.isLinux:
                self.requires("cpython/3.12.2" "#b4b4956dfba68645308b6aa9a4f1da04")
            else:
                self.requires("python/3.12.2" "#1d3af5c344b0c4e9246eada236db8f6b")

        if self.isLinux:
            if self.settings.arch == "x86_64":
                self.requires("libva/2.22.0" "#c3156ed8aeb0461f978b086681a2aa18")
                self.requires("intel-media-sdk/19.4" "#9dbce136887c6c03dfb9eabbefb05e44")
                self.requires("intel-onevpl/23.4.2" "#ef2169aa27c6c15928ffc717e9cb3f7f")
                self.requires("intel-gmmlib/22.5.2" "#a9a4be5e7f657758b6300e3b09074628")
                self.requires("intel-media-driver/23.4.3" "#f6c51eff39b7d59eeb75e7077025f0e8")
                self.requires("nv-codec-headers/12.1.14.0" "#65e2d80efd67e46fc41f135f2468e3df")
                self.requires("libmysqlclient/8.1.0" "#e762100664bad1c018ad71ecf702ea5e")

            if not self.isArm32:
                self._os_deps_package = "os_deps_for_desktop_linux"
                self.requires("os_deps_for_desktop_linux/ubuntu_focal" "#e3b3c4100f7d891449e13cb22ac44715")

        if self.haveDesktopClient:
            if self.isMacos:
                self.requires("hidapi/0.14.0" "#0effb18f67e2e098b013e0bc9983496e")
            self.requires("pathkit/m134" "#de17b62844ca3085dbeeb8c8fdea90c5")

        if self.isWindows or self.isMacos or self.isAndroid or self.isIos or (self.isLinux and not self.isArm32):
            self.requires("openal/1.24.3" "#bec0ba1235fbfba9af1eaf14d814e4b0")

        if self.isWindows:
            self.requires("directx/june2010" "#a0cbbd6a9cfef629fae6d2cf5a18bcdd")
            self.requires("intel-media-sdk-bin/2019r1-1" "#40cd824192475ce059cb9529a06a3e13")
            self.requires("msvc-redist/14.38.33135" "#716dc6c575a38ac17943d6f0f45dde6d")
            self.requires("winsdk-redist/10.0.22621.0" "#8dfc5bcbabe971a46f78c8d2657d7ec2")

        if self.isArm32 or self.isArm64:
            self.requires("sse2neon/1.7.0" "#188c824e380c1a5ad666d1229226b7c8")

        if self.settings.os in ("Android", "iOS") or self.haveAnalyticsServer:
            self.requires("libjpeg-turbo/3.0.3" "#79ebbd04ba41032d43a11f47c19de3b5")

        if self.haveDesktopClient or self.haveMediaserver:
            self.requires("flite/2.2" "#02b4c56a91ab5131673f3a79fc38b58c")

        if self.haveDesktopClient:
            self.requires("vms_help/" + VMS_HELP_VERSION)
            self.requires("quick_start_guide/" + QUICK_START_GUIDE_VERSION)
            self.requires("mobile_user_manual/" + MOBILE_USER_MANUAL_VERSION)

    def _dep_names(self):
        """Return set of dependency package names (Conan v2 API)."""
        return {dep.ref.name for _, dep in chain(
            self.dependencies.host.items(),
            self.dependencies.build.items())}

    def prepare_pkg_config_files(self):
        if self.isLinux:
            pc_files_dir = Path(self.generators_folder) / "os_deps_pkg_config"
            self.python_requires["os_deps_activator"].module.activate_pkg_config(
                self, pc_files_dir=pc_files_dir)

    def import_files_from_package(self, package, src, dst, glob):
        if package not in self._dep_names():
            return

        src_dir = Path(self.dependencies[package].package_folder) / src
        dst_dir = Path(self.generators_folder) / dst

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

        with open(Path(self.generators_folder) / "conan_imported_files.txt", "w") as file:
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

        if "vms_help" in self._dep_names():
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

        if self.isLinux or self.isWindows or self.isMacos or self.isAndroid:
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
    def isEmscripten(self):
        return self.settings.os == "Emscripten"

    @property
    def haveMediaserver(self):
        return not (self.isAndroid or self.isIos or self.isEmscripten)

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
        return Path(self.generators_folder) / "lib"
