# Metadata SDK

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

This package provides an SDK to create Analytics Plugins for the Video Management System (VMS).
Such plugins can analyze video streams and produce metadata.

From the business logic point of view, a typical Analytics Plugin is connected to a video camera,
may receive video frames from the camera or interact with the camera using a certain proprietary
way, and may generate metadata - events and objects (rectangles on a frame) - which is sent to the
Server to be stored in its database and visualized in the Client.

From the developers' point of view, an Analytics Plugin is a dynamic library (`.dll` on Windows,
`.so` on Linux) which exports a single `extern "C"` entry point function. Such function is a
factory for objects inherited from a dedicated SDK abstract class (in other words, implementing a
dedicated interface) `class nx::sdk::analytics::IPlugin` (`src/nx/sdk/analytics/i_plugin.h`),
derived from `class nx::sdk::IPlugin` (`src/nx/sdk/i_plugin.h`). This base interface also defines
the name and the prototype of the entry point function.

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
## Helper tools

To simplify implementation of a plugin, a number of helper classes are provided with this SDK that
implement the SDK interfaces and handle such complexities as reference counting and interface
requesting. Such classes are located in folders named `helpers`. If such a helper class does
not address all the requirements of the plugin author, it may be subclassed or not used at all -
the plugin can always implement everything from scratch using just interfaces from the header
files.

Also, some tools are provided with this SDK, which are recommended though not required to be used
by a plugin:

- `src/nx/sdk/helpers/` - utilities and typical/base classes implementing interfaces, including:
    - `ref_countable.h` - `class nx::sdk::RefCountable`: base class for objects implementing
        `IRefCountable` interface.
    - `ptr.h` - `class nx::sdk::Ptr`: Smart Pointer to reference-countable objects.
    - `to_string.h` and `src/nx/sdk/helpers/uuid_helper.h`: Conversion functions.

- `src/nx/sdk/analytics/helpers/` - Analytics-related utilities and typical/base classes
    implementing interfaces, including:
    - `plugin.h` - `class nx::sdk::analytics::Plugin`: Base class for a typical implementation of
        the `nx::sdk::analytics::IPlugin` interface; use it to start developing an Analytics
        Plugin.
    - `pixel_format.h`: Utilities for `enum IUncompressedFrame::PixelFormat`.

- Pure C++ library `nx_kit` (which itself is an independent project with its own
    [documentation](nx_kit/readme.md)) that offers convenient debug output (logging) and support
    for .ini files with options for experimenting (e.g. switching alternative implementations, or
    parameterizing the code), and a rudimentary framework for simple unit tests (used by the SDK
    itself, and plugin samples).

The recommended way to start developing a plugin is the following: inherit your classes from
`nx::sdk::analytics::Plugin`, `nx::sdk::analytics::Engine` and
`nx::sdk::analytics::ConsumingDeviceAgent` according to the Doxygen comments in those classes and
interfaces they implement.

---------------------------------------------------------------------------------------------------
## Samples: building and installing

This package includes two samples of an Analytics Plugin written in C++, located at `samples/`.

The first one is called `Sample Analytics Plugin`, located at `samples/sample_analytics_plugin/`.
It receives video frames from a camera, ignores them, and generates stub metadata Objects looking
as a rectangle moving diagonally across the frame. Also an Event is generated when the trajectory
finishes a loop. This plugin has many comments in the source code, which help to better understand
how Analytics Plugins should be written.

The second sample is called `Stub Analytics Plugin`, located at `samples/stub_analytics_plugin/`.
Its dynamic library declares a number of relatively small plugins. They ignore all the received
video frames from a camera, but demonstrate an extensive feature set - can generate various
metadata Objects and Events, and tend to use every feature available in the SDK. These Plugins have
certain settings which allow to test various SDK features. Some settings are declared in the
manifests and thus can be edited in the GUI, while some other settings which influence the certain
Plugin in general, including how to generate its manifest, are located in the .ini files (see
`stub_analytics_plugin_*_ini.h`), backed by the .ini file mechanism `nx/kit/ini_config.h`.

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
```

To compile the samples, and (if not cross-compiling) run unit tests, execute the commands collected
into the provided scripts (use their source code as a reference; run with `-h` or `/?` to see
the possible options):
```
# Windows, x64:
build_samples.bat

# Linux or Windows with Cygwin, x64:
build_samples.sh

# Linux, 64-bit ARM cross-compiling (e.g. Nvidia Tegra):
# NOTE: The provided file toolchain_arm64.cmake defines which cross-compiler will be used.
build_samples_arm64.sh

# Linux, 32-bit ARM cross-compiling (e.g. Raspberry Pi):
# NOTE: The provided file toolchain_arm32.cmake defines which cross-compiler will be used.
build_samples_arm32.sh
```

On Windows, after CMake generation phase, Visual Studio GUI can be used to compile a sample:
open `..\metadata_sdk-build\<sample_name>\<sample_name>.sln` and build the `ALL_BUILD` project.
Make sure that the platform combo-box is set to "x64".

After the successful build, locate the built artifacts:
```
# Windows:
..\metadata_sdk-build\<sample_name>\Debug\<sample_name>.dll

# Linux:
../metadata_sdk-build/<sample_name>/lib<sample_name>.so
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
