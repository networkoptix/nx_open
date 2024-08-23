# Video Source SDK (aka Camera Integration SDK)

// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

---------------------------------------------------------------------------------------------------
## License

The whole contents of this package, including all C/C++ source code, is licensed as Open Source
under the terms of Mozilla Public License 2.0: www.mozilla.org/MPL/2.0/, with the possible
exception of certain files which may be licensed under the terms of other open-source licenses
explicitly referenced in those files.

See the license texts in `licenses/` directory located in the root directory of this package.

---------------------------------------------------------------------------------------------------
## Introduction

This package provides an SDK to create so-called Camera Plugins for the Video Management System
(VMS).

From the business logic point of view, a Camera Plugin creates its own type of a Device (such
as a Camera or an I/O Module), capable of e.g. supplying video frames to the VMS Server.

Such a Plugin can, for example, be one of the following:

- Driver for the particular IP camera model, like the included sample `axis_camera_plugin`.
- Driver for the particular camera type, connecting to a camera in its own proprietary way, like
    the included sample `rpi_camera_plugin`.
- Driver for a virtual camera, like the included sample `image_library_plugin`.

From the developers' point of view, a Camera Plugin is a dynamic library (`.dll` on Windows,
`.so` on Linux) which exports a single `extern "C"` entry point function. Such function is a
factory for objects inherited from a dedicated SDK abstract class (in other words, implementing a
dedicated interface) `class nxpl::PluginInterface` (`src/plugins/plugin_api.h`). This base
interface also defines the name and the prototype of the entry point function.

ATTENTION: If you consider linking your plugin to any dynamic libraries, including the ones from
the OS, and the C++ standard library, consult
[src/nx/sdk/dynamic_libraries.md](@ref md_src_nx_sdk_dynamic_libraries) to avoid potential crashes.

To make it possible to develop plugins using a different C++ compiler (e.g. with an incompatible
ABI) rather than the one used to compile the VMS itself, or potentially in languages other than
C++, a COM-like approach is offered: all objects created in a plugin or passed to a plugin inherit
abstract classes declared in header files of this SDK, which have only pure virtual functions with
C-style arguments (not using C++ standard library classes). To manage lifetime of such objects,
they incorporate a reference counter.

The SDK C++ files have extensive documentation comments in Doxygen format, from which HTML files
are generated and included into the SDK: see `docs/html/index.html`.

---------------------------------------------------------------------------------------------------
## Samples: building and installing

This package includes a number of sample Camera Plugins written in C++, located at `samples/`.

These samples can be compiled and linked using CMake.

Prerequisites:
```
- CMake >= 3.14
- Windows (7 or 10): Microsoft Visual Studio >= 2019
    - ATTENTION: When linking to the C++ runtime dynamically (Visual Studio does it by default),
        avoid using the latest build tools (toolset) to avoid crashes - see the details in
        [src/nx/sdk/dynamic_libraries.md](@ref md_src_nx_sdk_dynamic_libraries).
- Linux (Ubuntu 18.04, 20.04, 22.04) including ARM (e.g. Raspberry Pi or Nvidia Tegra) native or
    cross-compiling:
    - g++ >= 7.5
    - make or Ninja
- Qt5 (required by certain samples only)
- Raspberry Pi sysroot (required by certain samples only)
```

To compile the samples, execute the commands collected into the provided scripts (use their source
code as a reference; run with `-h` or `/?` to see the possible options):
```
# Windows, x64:
build_samples.bat

# Linux or Windows with Cygwin, x64:
build_samples.sh

# Linux, 64-bit ARM cross-compiling (e.g. Nvidia Tegra):
# NOTE: The provided file toolchain_arm64.cmake defines which cross-compiler will be used.
build_samples_arm64.sh

# Linux, 32-bit ARM cross-compiling, without Raspberry Pi specific samples:
# NOTE: The provided file toolchain_arm32.cmake defines which cross-compiler will be used.
build_samples_arm32.sh

# Linux, Raspberry Pi (32-bit ARM) cross-compiling, with samples specific to Raspberry Pi:
# NOTE: The provided file toolchain_arm32.cmake defines which cross-compiler will be used.
build_samples_rpi.sh
```

ATTENTION: Some of the samples require the Qt library. The Qt version should be the same as the one
that comes with the Server (see the installed Server files), otherwise an unpredictable behavior
may take place. If CMake cannot find Qt, specify its path to a build_samples* script (will be
passed to cmake), or choose to skip Qt-based samples via `--no-qt-samples` option, as shown below:
```
# Windows:
# Note the quotes - otherwise, .bat interpreter will split the arg into two by "=".
build_samples.bat "-DCMAKE_PREFIX_PATH=<full_path_to_Qt5_dir>"
# or
build_samples.bat --no-qt-samples

# Linux:
./build_samples.sh -DCMAKE_PREFIX_PATH=<full_path_to_Qt5_dir>
# or
./build_samples.sh --no-qt-samples
```

On Windows, after CMake generation phase, Visual Studio GUI can be used to compile a sample:
open `..\video_source_sdk-build\<sample_name>\<sample_name>.sln` and build the `ALL_BUILD` project.
Make sure that the platform combo-box is set to "x64".

After the successful build, locate the built artifacts:
```
# Windows:
..\video_source_sdk-build\<sample_name>\Debug\<sample_name>.dll

# Linux:
../video_source_sdk-build/<sample_name>/lib<sample_name>.so
```

To install a plugin, just copy its library file to the dedicated folder in the VMS Server
installation directory:
```
# Windows:
C:\Program Files\<vms-installation-dir>\MediaServer\plugins\

# Linux:
/opt/<vms-installation-dir>/mediaserver/bin/plugins/
```
ATTENTION: After copying a plugin library, the Server has to be restarted.

---------------------------------------------------------------------------------------------------
## Compatibility and Breaking Changes

Plugin libraries compiled using nx_sdk 1.7.1 (which has been included with VMS 3.2) are compatible
with VMS 4.0, because the binary interface (ABI) did not change. The same way, plugins compiled
with this newer video_source_sdk should work with VMS 3.2 as well.

The current video_source_sdk version (coming with VMS 4.0) has no new features or extensions to the
API available for the plugins in nx_sdk 1.7.1 (coming with VMS 3.2).

Re-compiling the source code of plugins written with nx_sdk 1.7.1 using this newer video_source_sdk
requires some simple adjustments due the following breaking changes in SDK source code:

- Changed prototypes of the base interface `class PluginInterface` methods (does not affect binary
    compatibility) `addRef()` and `releaseRef()`:
    - Now they return `int` instead of `unsigned int`.
    - Now they are `const`.
- Removed unused interface `nxpl::Plugin3` which only added `setLocale()` to `nxpl::Plugin2`.
- SDK headers moved:
    - `include/plugins/camera_*.h` -> `src/camera/`.
    - `include/plugins/plugin_*.h` -> `src/plugins/`.
- Helper utilities, formerly in `plugin_tools.h`:
    - `ScopedRef<>` replaced with `Ptr<>` (`src/nx/sdk/ptr.h`).
    - `alignUp()`, `mallocAligned()`, `freeAligned()` moved to `nx/kit/utils.h`.
