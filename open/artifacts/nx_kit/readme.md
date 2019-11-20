// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

# C++ library `nx_kit` by Network Optix

---------------------------------------------------------------------------------------------------
## Introduction

`nx_kit` is a kit of pure C99/C++11 platform-agnostic utils used across various projects.

This kit does not use any non-standard libraries. There is a mention of Qt but only conditional: if
Qt is used by a project which uses nx_kit, additional convenience (e.g. supporting certain Qt types
in debug utilities) is provided. Similarly, while being fully cross-platform, if nx_kit detects one
of the familiar platforms, additional convenience or customized behavior is offered.

Currently, the following units are included into `nx_kit`:

- `nx::kit::IniConfig` - `nx/kit/ini_config.h`
   A mechanism for configuration variables that can alter the behavior of the code for the purpose
   of debugging and experimenting, with insignificant performance overhead. The default (initial)
   values are defined in the code and lead to the nominal behavior, which can be overridden by
   creating ini files (with name=value lines) in the system temp directory (or /sdcard on Android).

- `nx::kit::debug` - `nx/kit/debug.h`
   Utilities for debugging: measuring execution time and FPS, logging values and messages, etc.

- `nx::kit::test` - `nx/kit/test.h`
   A rudimentary standalone unit testing framework designed to mimic Google Test to a certain
   degree. Used for the unit tests for `ini_config`, `debug` and `utils` units of `nx_kit`.

- `nx::kit::utils` - `nx/kit/utils.h`
   Simple utilities used by other `nx_kit` units.

- `nx::kit::Json` - `nx/kit/json11.h`
  Json parser/writer "Json11" originally written by DropBox: https://github.com/dropbox/json11
  Its source code (without additional unneeded files) is located unchanged in `nx_kit/src/json11`,
  but should be used via `nx/kit/json11.h` wrapper which puts `class Json` into `namespace nx::kit`
  and makes it exported from nx_kit dynamic library.

---------------------------------------------------------------------------------------------------
## Building

This kit is suitable to form a source-only artifact `nx_kit` with the only `src` folder in it -
the source code files will be compiled as part of the project that uses the artifact. For cmake
users, `CMakeLists.txt` can also be deployed to the artifact to be used in `add_subdirectory()`.

All classes and functions in this kit are prefixed with `NX_KIT_API` macro which allows to control
symbol visibility if compiled as a dynamic library. See its usage in `CMakeLists.txt` for details.

Tests for these utils are located in the `unit_tests` folder. The test project can be built and run
on Linux using cmake/ctest, on Windows with Cygwin using cmake/ctest or CLion IDE, or on Windows
using Microsoft Visual Studio 2012+.

To build and run tests on Linux or Windows+Cygwin, with `CMake >= 3.3.2` and `g++ >= 4.8.4`:
```
# Make a folder for build results:
mkdir .../nx_kit-build
cd .../nx_kit-build

# Generate Makefiles:
cmake .../nx_kit

# Compile and link:
cmake --build .

# Run tests:
./nx_kit_ut
```
