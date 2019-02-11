Network Optix Analytics SDK

---------------------------------------------------------------------------------------------------
# License

The whole contents of this package, including all C/C++ source code, is licensed strictly under the
terms negotiated in written with the party that has officially received this package from Network
Optix, Inc.

---------------------------------------------------------------------------------------------------
# Introduction

This package provides an SDK to create so-called Analytics Plugins for Nx Witness VMS.

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

To make it possible to develop plugins using a different C++ compiler (e.g. with an incompatible
ABI) rather than the one used to compile Nx Witness VMS, or potentially in languages other than
C++, a COM-like approach is offered: all objects created in a Plugin or passed to a Plugin inherit
abstract classes declared in header files of this SDK, which have only pure virtual functions with
C-style arguments (not using C++ standard library classes). To manage lifetime of such objects,
they incorporate a reference counter.

The SDK C++ files have extensive documentation comments in Doxygen format, from which HTML files
are generated and included into the SDK: see `docs/html/index.html`.

---------------------------------------------------------------------------------------------------
# Helper tools

To simplify implementation of a Plugin, a number of helper classes are provided with this SDK that
implement the SDK interfaces and handle such complexities as reference counting and interface
requesting. Such classes are located in folders named `helpers`. If such a helper class does
not address all the requirements of a Plugin author, it may be subclassed or not used at all - a
Plugin can always implement everything from scratch using just interfaces from the header files.

Also, some tools are provided with this SDK in `helpers` folders, which are recommended though not
required to be used by a Plugin:

- `src/nx/sdk/ptr.h` - `class nx::sdk::Ptr`: Smart Pointer to objects implementing interfaces.

- `src/nx/sdk/to_string.h` and `src/nx/sdk/uuid_helper.h`: Conversion and other helper functions
    for various SDK classes.

- Pure C++ library `nx_kit` (which itself is an independent project with its own documentation)
    that offers convenient debug output (logging) and support for .ini files with options for
    experimenting (e.g. switching alternative implementations, or parameterizing the code), and a
    rudimentary framework for simple unit tests (used by the SDK itself, and Plugin samples).

---------------------------------------------------------------------------------------------------
# Sample: building and installing

This package includes a sample Analytics Plugin written in C++: `Stub Analytics Plugin`, located at
`samples/stub_analytics_plugin/`. It receives video frames from a camera, ignores them, and
generates stub metadata Objects looking as a rectangle moving diagonally across the frame, and also
some hard-coded Events. This plugin has certain settings which allow to test various SDK features,
see `stub_analytics_plugin_ini.h` for details.

The included `Stub Analytics Plugin` source files can be compiled and linked using CMake, yielding
`stub_analytics_plugin.dll` on Windows, or `libstub_analytics_plugin.so` on Linux.

Prerequisites:
```
- CMake >= 3.3.2
- Windows (7 or 10): Microsoft Visual Studio >= 2015
- Linux (Ubuntu 16.04 or 18.04), including Nvidia Tegra native or cross-compiling:
    - g++ >= 5.4.0
    - make or Ninja
```

To compile the sample, and (if not cross-compiling) run unit tests, execute the commands collected
into the provided scripts (use their source code as a reference):
```
# Windows, x64:
build_sample.bat

# Linux or Windows with Cygwin, x64:
build_sample.sh

# Nvidia Tegra cross-compiling (64-bit ARM):
# NOTE: The file nvidia_tegra_toolchain.cmake defines which cross-compiler will be used.
build_sample_nvidia_tegra.sh
```

On Windows, after CMake generation phase, Visual Studio GUI can be used to compile the sample:
open `../analytics_sdk-build/stub_analytics_plugin.sln` and build the `ALL_BUILD` project. Make
sure that the platform combo-box is set to "x64".

After successful building, locate the main built artifact:
```
# Windows:
..\analytics_sdk-build\Debug\stub_analytics_plugin.dll

# Linux:
../analytics_sdk-build/libstub_analytics_plugin.so
```

To install the plugin, just copy its library file to the dedicated folder in the Nx Witness
Server installation directory:
```
# Windows:
C:\Program Files\Network Optix\Nx Witness\MediaServer\plugins\

# Linux:
/opt/networkoptix/mediaserver/bin/plugins/
```
ATTENTION: After copying the plugin library, the Server has to be restarted.
