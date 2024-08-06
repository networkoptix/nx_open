# The nx_kit library

// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

---------------------------------------------------------------------------------------------------
## Introduction

The nx_kit is a kit of pure C99/C++11 platform-agnostic utilities useful for experimenting and
debugging.

This kit does not use any non-standard libraries. There is a mention of the Qt library, but only
conditional: if Qt is used by a project which uses nx_kit, additional convenience (e.g.
supporting certain Qt types in debug utilities) is provided. Similarly, while being fully
cross-platform, if nx_kit detects one of the familiar platforms, additional convenience or
customized behavior is offered.

Currently, the following units are included into nx_kit:

- `nx::kit::IniConfig` - `nx/kit/ini_config.h`
   A mechanism for configuration variables that can alter the behavior of the code for the purpose
   of debugging and experimenting, with insignificant performance overhead. The default (initial)
   values are defined in the code and lead to the nominal behavior, which can be overridden by
   creating ini files (with name=value lines) in the system temp directory (or /sdcard on Android).

- `nx::kit::debug` - `nx/kit/debug.h`
   Utilities for debugging: measuring execution time and FPS, logging values and messages, etc.

- `nx::kit::test` - `nx/kit/test.h`
   A rudimentary standalone unit testing framework designed to mimic Google Test to a certain
   degree. Used for the unit tests for `ini_config`, `debug` and `utils` units of nx_kit.

- `nx::kit::utils` - `nx/kit/utils.h`
   Simple utilities used by other nx_kit units.

- `nx::kit::Json` - `nx/kit/json11.h`
  Json parser/writer "Json11" originally written by DropBox: https://github.com/dropbox/json11
  Its source code (without additional unneeded files) is located unchanged in `nx_kit/src/json11`,
  but should be used via `nx/kit/json11.h` wrapper which puts `class Json` into `namespace nx::kit`
  and makes it exported from nx_kit dynamic library.

---------------------------------------------------------------------------------------------------
## Building

This kit is suitable to form a source-only artifact `nx_kit` with the only `src` folder in it - the
source code files will be compiled as part of the project that uses the artifact. For the CMake
users, `CMakeLists.txt` can also be deployed to the artifact to be used in `add_subdirectory()`.

All classes and functions in this kit are prefixed with `NX_KIT_API` macro which allows to control
symbol visibility if compiled as a dynamic library. See its usage in `CMakeLists.txt` for details.

Tests for these utils are located in the `unit_tests` folder. The test project can be built and run
on Linux using cmake/ctest, on Windows with Cygwin using cmake/ctest or CLion IDE, or on Windows
using Microsoft Visual Studio 2012+.

For convenience and as an example, there are scripts named `build.bat` and `build.sh`. These
scripts build and run `nx_kit` library and its unit tests for Windows and Linux (or
Windows + Cygwin / MinGW / Git Bash), respectively. They serve as wrappers around the standard
CMake build procedure, including the generation stage, building and running ctest.

Prerequisites:
```
- CMake >= 3.14
- Windows (7, 10 or 11): Microsoft Visual Studio >= 2012
- Linux (Ubuntu 18.04, 20.04, 22.04)
    - g++ >= 7.5
    - make or Ninja
```

Please note that on the Windows platform, if you use one of the provided scripts, you should also
have Ninja installed as part of Microsoft Visual Studio. Additionally, if `build.bat` is
executed within the `nx` repository directory, it attempts to initialize the environment under the
assumption that the Microsoft Visual Studio 2022 Community edition is being used. If any of these
conditions (MSVC version or `nx_kit` directory placement) are not met, the build must be initiated
from an already initialized environment (e.g., the "x64 Native Tools Command Prompt for VS 2022"
for MSVC 2022, or its equivalent for the MSVC version installed on your machine).

To view the possible command-line options of the scripts, run them with `-h` or `/?`.
