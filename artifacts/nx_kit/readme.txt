# Copyright 2017 Network Optix, Inc. Licensed under GNU Lesser General Public License version 3.

---------------------------------------------------------------------------------------------------
- Introduction

"nx_kit" is a kit of pure C/C++ utils written by Nx and used across various Nx projects. This kit
does not use any non-standard libraries.

Currently, the following components are included into "nx_kit".

- nx::kit::IniConfig - ini_config.h

 * Mechanism for configuration variables that can alter the behavior of the code for the purpose of
 * debugging and experimenting, with very little performance overhead. The default (initial) values
 * are defined in the code and lead to the nominal behavior, which can be overridden by creating
 * .ini files (with name=value lines) in the system temp directory (or /sdcard on Android).

- nx::kit::debug - debug.h

 * Utilities for debugging: measuring execution time and FPS, working with strings, logging values
 * and messages.

---------------------------------------------------------------------------------------------------
- Building

This kit is suitable to form a source-only artifact "nx_kit" with the only "src" folder in it -
the source code files will be compiled as part of the project that uses the artifact. For cmake
users, "nx_kit.cmake" can also be deployed to the artifact to be included into cmake projects.

All classes and functions in this kit are prefixed with NX_KIT_API macro which allows to control
symbol visibility if compiled as a dynamic library. See its usage in "nx_kit.cmake" for details.

Tests for these utils are located in the "test" folder, and should not be deployed to the artifact.
The test project can be built and run on Linux or Windows with Cygwin, using cmake or CLion IDE.

To build and run tests on Linux, with CMake version >= 3.3.2, g++ version 4.8.4:

# Make a folder for build results.
mkdir .../nx_kit-build
cd .../nx_kit-build

# Generate Makefiles.
cmake .../nx_open/artifacts/nx_kit

# Compile and link.
cmake --build .

# Run tests.
./nx_kit_test
