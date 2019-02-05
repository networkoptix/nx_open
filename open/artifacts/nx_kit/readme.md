---------------------------------------------------------------------------------------------------
# Introduction

`nx_kit` is a kit of pure C99/C++11 platform-agnostic utils written by Network Optix (Nx) and used
across various Nx projects.

This kit does not use any non-standard libraries. There is a mention of Qt but only conditional: if
Qt is used by a project which uses nx_kit, additional convenience (e.g. supporting certain Qt types
in debug utilities) is provided. Similarly, while being fully cross-platform, if nx_kit detects one
of the familiar platforms, additional convenience or customized behavior is offered.

Currently, the following components are included into `nx_kit`:

- `nx::kit::IniConfig` - `ini_config.h`
   Mechanism for configuration variables that can alter the behavior of the code for the purpose of
   debugging and experimenting, with very little performance overhead. The default (initial) values
   are defined in the code and lead to the nominal behavior, which can be overridden by creating
   ini files (with name=value lines) in the system temp directory (or /sdcard on Android).

- `nx::kit::debug` - `debug.h`
   Utilities for debugging: measuring execution time and FPS, working with strings, logging values
   and messages.

- `nx::kit::test` - `test.h`
   Rudimentary standalone unit testing framework designed to mimic Google Test to a certain degree.
   This framework is used for the unit tests for `ini_config` and `debug` units of `nx_kit`.

---------------------------------------------------------------------------------------------------
# Building

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
