TODO: Add copyright notice.

This is a kit of pure C/C++ utils written by Nx and used across various Nx projects. This kit does
not use any non-standard libraries.

This kit is suitable to form a source-only artifact "nx_kit" with the only "src" folder in it -
the source code files will be compiled as part of the project that uses the artifact. For cmake
users, "nx_kit.cmake" can also be deployed to the artifact to be included into cmake projects.

All classes and functions in this kit are prefixed with NX_KIT_API macro which allows to control
symbol visibility if compiled as a dynamic library. See its usage in "nx_kit.cmake" for details.

Tests for these utils are located in the "test" folder, and should not be deployed to the artifact.
The test project can be built and run on Linux or Windows with Cygwin, using cmake or CLion IDE.
