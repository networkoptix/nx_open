# Cloud Storage SDK

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

This package provides an SDK to create so-called Cloud Storage Plugins for the Video Management
System (VMS).

From the business logic point of view, an Cloud Storage Plugin creates its own type of a Storage
capable of receiving the video stream from the VMS Server to be recorded into some storage in order
to be retrieved afterwards.

This SDK is conceived as an alternative to the long-existing Storage SDK for those cases when the
underlying implementation is stream-based rather than file-based. Plugins created with this SDK are
used seamlessly by the VMS along with Storage plugins - from the VMS user point of view, both types
of plugins create the video recording and playback looks the same whether the conventional storages
or stream based are used.

Here are the main ideas behind this SDK:
* It enables plugin to write and read chunks of media data corresponding to the given device and
stream index to and from the backend. Chunk is essentially a part of continuos media data from
device. Server will cut this incoming stream into parts which in turn will be fed and afterwards
read from the plugin on the per packet basis.
* Media stream consists of media data packets. Each packet has some typed attributes such as
timestamp and duration and opaque array of binary data. Plugin does not need to know the structure
of the provided binary packet data. It should store it as is and return back when requested.
* Apart from handling continuos media data SDK provides interface for storing and requesting
various types of metadata such as object detection data, motion and bookmarks. This data is
provided in the form of JSON text objects. The SDK contains helper C++ data structs which
correspond to the each of the JSONs provided. The JSON text representation of those objects is
chosen from the performance perspective to eliminate the need for excess serialization and
deserialization of data since plugin itself should generally be only a thin layer between Server
and backend which should only transmit the data and not perform any computations itself.

ATTENTION: This SDK is currently considered a work-in progress. Plugins written with this SDK are
not recommended for production systems. This SDK is likely to have breaking changes introduced in
the further VMS versions, so the plugins written now will likely not be compatible with the future
VMS versions. As part of this work-in-progress, the SDK contains several header files taken from
Video Source SDK - `camera/camera_plugin.h`, `camera/camera_plugin_types.h` and
`plugins/plugin_api.h`; the entities in these files are named using outdated conventions and may
lack proper documentation.

From the developers' point of view, an Cloud Storage Plugin is a dynamic library (`.dll` on
Windows, `.so` on Linux) which exports a single `extern "C"` entry point function. Such function is
a factory for objects inherited from a dedicated SDK abstract class (in other words, implementing a
dedicated interface) `class nx::sdk::cloud_storage::IPlugin`
(`src/nx/sdk/cloud_storage/i_plugin.h`), derived from `class nx::sdk::IPlugin`
(`src/nx/sdk/i_plugin.h`). This base interface also defines the name and the prototype of the entry
point function.

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
    - `to_string.h` and `uuid_helper.h`: Conversion functions.

- Pure C++ library `nx_kit` (which itself is an independent project with its own
    [documentation](nx_kit/readme.md)) that offers convenient debug output (logging) and support
    for .ini files with options for experimenting (e.g. switching alternative implementations, or
    parameterizing the code), and a rudimentary framework for simple unit tests (used by the SDK
    itself, and plugin samples).

---------------------------------------------------------------------------------------------------
## Samples: building and installing

This package includes two samples of an Cloud Storage Plugin written in C++, located at
`samples/`.

The first one is called `Sample Cloud Storage Plugin`, located at
`samples/sample_cloud_storage_plugin/`. It receives the video stream from the Server, ignores
it, and returns nothing when the Server tries to retrieve the video stream from the Storage. This
plugin has many comments in the source code, which help to better understand how Cloud Storage
Plugins should be written.

The second sample is called `Stub Cloud Storage Plugin`, located at
`samples/stub_cloud_storage_plugin/`. It works using the local file system as the underlying
storage, and tends to use every feature available in the SDK.

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

# Linux, 32-bit ARM cross-compiling (e.g. Raspberry Pi):
# NOTE: The provided file toolchain_arm32.cmake defines which cross-compiler will be used.
build_samples_arm32.sh
```

On Windows, after CMake generation phase, Visual Studio GUI can be used to compile a sample:
open `..\cloud_storage_sdk-build\<sample_name>\<sample_name>.sln` and build the `ALL_BUILD`
project. Make sure that the platform combo-box is set to "x64".

After the successful build, locate the built artifacts:
```
# Windows:
..\cloud_storage_sdk-build\<sample_name>\Debug\<sample_name>.dll

# Linux:
../cloud_storage_sdk-build/<sample_name>/lib<sample_name>.so
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
