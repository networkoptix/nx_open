Network Optix Video Analytics SDK

---------------------------------------------------------------------------------------------------
# License

The whole contents of this package, including all C/C++ source code, is licensed strictly under the
terms negotiated in written with the party that has officially received this package from Network
Optix, Inc.

---------------------------------------------------------------------------------------------------
# Introduction

This package provides an SDK to create video analytics plugins for Nx Witness VMS. A plugin should
be created in the form of a dynamic library (`.dll` on Windows, `.so` on Linux) which exports a
single `extern "C"` function - a factory for objects inherited from a dedicated SDK abstract class
(in other words, implementing a dedicated interface) `class PluginInterface` declared in
`src/plugins/plugin_api.h`.

From business logic point of view, such Metadata Plugin is connected to a video camera, may receive
video frames from the camera or interact with the camera using a certain proprietary way, and may
generate metadata - events and objects (rectangles on a frame) - which is sent to Mediaserver to be
stored in its database and visualized in Client.

To make it possible to develop plugins using a different C++ compiler (e.g. with incompatible ABI)
rather than the one used to compile Nx Witness VMS, or potentially in languages other than C++,
a COM-like approach is offered: all objects created in a plugin inherit abstract classes declared
in header files of this SDK, which have only pure virtual functions with C-style arguments (not
using C++ standard library classes). To manage lifetime of such objects, they have a reference
counter.

---------------------------------------------------------------------------------------------------
# Helper tools

To simplify implementation of a plugin, a number of helper classes are provided with this SDK that
implement the SDK interfaces and handle such complexities as reference counting and interface
requesting. Such classes are typically named starting with `Common`. If such a helper class does
not address all the requirements of a plugin author, it may be subclassed or not used at all - a
plugin can always implement everything from scratch using just interfaces from header files.

Also, some tools are provided with this SDK, which are recommended though not required to be used:

- Header-only library `src/plugins/plugin_tools.h` which handles reference counting and GUID
transforming.

- Pure C++ library `nx_kit` (which itself is an independent project with its own documentation)
that offers convenient debug output (logging) and support for .ini files with options for
experimenting (e.g. switching alternative implementations, or parameterizing the code), and a
rudimentary framework for simple unit tests.

---------------------------------------------------------------------------------------------------
# Sample: building and installing

This package includes a sample plugin written in C++ using this SDK: `Stub Metadata Plugin`,
located at `samples/stub_metadata_plugin/`. It receives video frames from a camera, ignores them,
and generates stub metadata objects looking as a rectangle moving diagonally across the frame, and
also some hard-coded events.

The included `Stub Metadata Plugin` source files can be compiled and linked using CMake, yielding
`stub_metadata_plugin.dll` on Windows, or `libstub_metadata_plugin.so` on Linux.

Prerequisites:
```
CMake >= 3.3.2
Windows: Microsoft Visual Studio >= 2015
Linux (including Nvidia Tegra native compiling): gcc >= 7.3, make or Ninja (recommended)
Nvidia Tegra cross-compiling: sysroot, arm-64 gcc >= 7.3 (e.g. Linaro), make or Ninja (recommended)
```

First, create a build directory at any convenient location:
```
mkdir .../metadata_sdk-build
cd .../metadata_sdk-build
```

Then, generate build system files (they depend on the platform and chosen build tool) via CMake:
```
# Windows - generating .vcxproj and .sln:
cmake ...\metadata_sdk\samples\stub_metadata_plugin -Ax64

# Linux - generating Ninja files:
cmake .../metadata_sdk/samples/stub_metadata_plugin -GNinja

# Linux - generating makefiles:
cmake .../metadata_sdk/samples/stub_metadata_plugin

# Nvidia Tegra cross-compiling (64-bit ARM) - generating Ninja files:
# NOTE: Specify SYSROOT_DIR and TOOLCHAIN_DIR (e.g. Linaro 7) in nvidia_tegra_toolchain.cmake.
cmake .../metadata_sdk/samples/stub_metadata_plugin -GNinja \
    -DCMAKE_TOOLCHAIN_FILE=.../nvidia_tegra_toolchain.cmake
```

Finally, compile and link the library:
```
# Windows or Linux via command line:
cmake --build .

# Windows via Visual Studio GUI:
# Open .../build/stub_metadata_plugin.sln and build ALL_BUILD project.
```

Locate the main built artifact:
```
# Windows:
...\metadata_sdk-build\Debug\stub_metadata_plugin.dll

# Linux:
.../metadata_sdk-build/libstub_metadata_plugin.so
```

The above steps are automated in the provided scripts:
```
# Windows:
...\metadata_sdk\build_sample.bat

# Linux or Windows with Cygwin:
.../metadata_sdk/build_sample.sh

# Nvidia Tegra cross-compiling (64-bit ARM):
# NOTE: Specify SYSROOT_DIR and TOOLCHAIN_DIR (e.g. Linaro 7) in nvidia_tegra_toolchain.cmake.
.../metadata_sdk/build_sample_nvidia_tegra.sh
```

To install the plugin, just copy its library file to the dedicated folder in the Nx Witness
Mediaserver installation directory:
```
# Windows, portable installation:
...\nx_vms-win64\plugins\

# Windows, default installation path:
C:\Program Files\Network Optix\Nx Witness\MediaServer\plugins\

# Linux, default installation path:
/opt/networkoptix/mediaserver/bin/plugins/
```
ATTENTION: After copying the plugin library, Mediaserver has to be restarted.
