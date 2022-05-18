# Nx Meta VMP Open Source Components

// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

---------------------------------------------------------------------------------------------------
## Introduction

This repository `nx_open` contains **Network Optix Meta Video Management Platform open source
components** - the source code and specifications which are part of the Nx Meta Video Management
Platform (VMP) which in turn is used to build all Powered-by-Nx products, including Nx Witness
Video Management System (VMS).

Currently, the main VMS component which can be built from this repository is the Desktop Client.
Other notable Components which are parts of the Desktop Client, but can be useful independently,
include the Nx Kit library (`artifacts/nx_kit/`) - see its `readme.md` for details.

Most of the source code and other files are licensed under the terms of Mozilla Public License 2.0
(unless specified otherwise in the files) which can be found in the `license_mpl2.md` file in the
`licenses/` directory in the root directory of the repository.

---------------------------------------------------------------------------------------------------
## Build environment

Currently the following platforms and architectures are supported by this repository:
- Windows x64 (Microsoft Visual Studio 2019).
- Linux x64 (GCC or Clang).
- Linux 64-bit ARM (cross-compiling on Linux x64, GCC or Clang).
- Linux 32-bit ARM (cross-compiling on Linux x64, GCC or Clang).
- MacOS (Xcode with Clang).

- **Windows**: this guide implies Windows 10 x64 - other versions may require some adaptation. The
    build can be triggered from the command line of `cmd`, MinGW (Git Bash), or **Cygwin**.
- **Linux**: this guide implies Debian 10 or Ubuntu (18.04 or 20.04) - other flavors/versions may
    require some adaptation.

Here we describe the pre-requisites which are needed to build all of the Components.

NOTE: Certain Components in this repository can be built using more platforms and compilers, e.g.
the Nx Kit library (`artifacts/nx_kit/`).

Pre-requisites per platform (the details are given in the further subsections):
- CMake 3.19.0+ (on Windows, comes with Microsoft Visual Studio 2019).
- Ninja (on Windows, comes with Miscosoft Visual Studio 2019).
- Python 3.8+.
- Conan 1.42.x.
- For Linux:
    - `chrpath`.
    - Certain packages for the build dependencies.
- For Windows:
    - Microsoft Visual Studio 2019 Community Edition.

### CMake

The Components use CMake as a build system. CMake 3.19.0+ must be installed (the latest stable
version is recommended) and be available on `PATH` as `cmake`.

- **Linux**: either from https://cmake.org/download/, or from the official Kitware APT repository
    for Debian and Ubuntu (https://apt.kitware.com/). It is recommended to completely remove
    (`apt purge -y --auto-remove cmake`) the CMake package from the Main repository before
    installing the one from the Kitware repository.
- **Windows**: CMake which comes with Microsoft Visual Studio 2019 is suitable. If desired, a
    standalone installation of CMake can be performed from https://cmake.org/download/; after
    installing, choose "Add CMake to the system PATH for all users" and add the path to `cmake.exe`
    to `CMakeSettings.json` (see the "Generation" section of this guide).
    - ATTENTION: If you use Cygwin, make sure the Cygwin's `cmake` is not on `PATH`.

### Python

Python 3.8+ should be installed and available on `PATH` either as `python` or `python3`.

- **Windows**: a Windows-native (non-Cygwin) version of Python should be installed. A Cygwin
    version of Python may be installed as well, but must appear on `PATH` as a symlink (this makes
    it not visible to Windows-native programs including `cmake`).
- Python module `pyaml` should be installed into Python:
    ```
    python -m pip install pyaml # via pip
    sudo apt install python3-yaml # via apt
    ```
- **Linux**: When using `apt`, `distutils` must be installed explicitly:
    ```
    sudo apt install python3-distutils
    ```
    Also, `python` should refer to version 3.8+ (e.g. by installing the `python-is-python3`
    package).

### Conan

Conan 1.42.x should be installed. It is recommended to install Conan by Python's `pip`:
```
python -m pip install conan==1.42.2
```

### Build tools

- **Windows**: Install [Visual Studio 2019 Community Edition](https://visualstudio.microsoft.com/downloads/)
    **The Workload** "Desktop development with C++" should be installed. Also make sure that in
    **Individual components**, "C++ CMake tools for Windows" is selected.

- **Linux**:
    - Install the `chrpath` tool.
    - Install Ninja build tool using either way:
        - From https://ninja-build.org/; make sure `ninja` is on `PATH`.
        - On Debian/Ubuntu - install via apt:
            ```
            sudo apt install ninja-build
            ```
    - Install the following build dependencies:
        - zlib: `zlib1g-dev`
        - OpenAL: `libopenal-dev`
        - Mesa 3D: `mesa-common-dev`
        - Mesa 3D GLX and DRI: `libgl1-mesa-dev`
        - Mesa 3D GLU: `libglu1-mesa-dev`
        - LDAP: `libldap2-dev`
        - Xfixes: `libxfixes-dev` (used for cursors)
        - X11 Screen Saver extension library: `libxss-dev`
        - GStreamer: `libgstreamer1.0-0` (a runtime dependency)
        - GStreamer plugins: `libgstreamer-plugins-base1.0-0` (a runtime dependency)
        - XSLT library: `libxslt1.1` (a runtime dependency, required by Qt)
        - Zip archiver: `zip` (command, required for ARM distribution building)

        A typical command to install all of the above:
        ```
        sudo apt install zlib1g-dev libopenal-dev mesa-common-dev libgl1-mesa-dev \
            libglu1-mesa-dev libldap2-dev libxfixes-dev libxss-dev libgstreamer1.0-0 \
            libgstreamer-plugins-base1.0-0 libxslt1.1 pkg-config
        ```

---------------------------------------------------------------------------------------------------
## Using CMake

We recommend doing out-of-source builds with CMake – the source code folder (VCS folder) remains
intact, and all build results go into a dedicated folder. E.g. sources are in `nx_open/`, the build
goes to `nx_open-build/`.

With CMake, building is done in two stages:
- Generation stage: performed by cmake.
- Build stage: performed by a build tool chosen at the Generation stage - we use `ninja` for both
    Linux and Windows platforms. Can be also invoked via the CMake executable.

After the generation is performed once, further building attempts are performed invoking the chosen
build tool - no need to manually invoke the Generation stage again, because the generated rules for
the build tool include calling CMake Generation stage when necessary, e.g. when the CMake or source
files change. Thus, generally, invoking the Generation stage should be done only once - after the
initial cloning of the repository.

### Under the hood

During the Build stage the following things happen:
1. Pre-build actions are executed. The script `ninja_tool.py` reads the file `pre_build.ninja_tool`
    in the build directory and executes the commands from this file. The exact command list depends
    on the build platform and generation options and can include:
    - Cleaning the build directory from the files which are no longer built.
    - Patching build.ninja file (changing dependencies, adding new commands, etc.).
    - Scanning for third-party package changes and fetching the new versions of the packages.
    - Other preliminary actions.
2. The build itself. Ninja reads its configuration files (`build.ninja` and `rules.ninja`) which
    can be patched by `ninja_tool.py`, and builds the project. In some cases (new source files,
    package updates, etc.) for the correct build the project needs to be re-generated. If such need
    arises, `ninja` will run `cmake` to perform the Generation stage and then return to the Build
    stage again.

---------------------------------------------------------------------------------------------------
## Building VMS Desktop Client

To build the Client, besides cloning this repository, you must obtain a Customization Package -
it is a collection of texts and graphics used in the GUI; it defines the branding of the VMS. The
Customization Package comes as a zip file.

If you want to build an Nx-Meta-branded VMS Client, download the Nx Meta customization package from
https://meta.nxvms.com/. Otherwise, download a different Powered-by-Nx customization package using
the corresponding Developer account.

All the commands necessary to perform the CMake Configuration and Build stages are written in the
scripts `build.sh` (for Linux and MacOS) and `build.bat` (for Windows) located in the repository
root. Please treat these scripts as a quick start aid, study their source, and feel free to use
your favorite C++ development workflow instead.

The scripts create/use the build directory as a sibling to the repository root directory, having
added the `-build` suffix. Here we assume the repository root is `nx_open/`, so the build directory
will be `nx_open-build/`.

When running the scripts with the build directory absent, they create it, perform the CMake
Generation stage (which creates `CMakeCache.txt` in the build directory), and then perform the
Build stage. The same takes place when the build directory exists but `CMakeCache.txt` in it is
absent. When running the scripts with `CMakeCache.txt` in the build directory present, they only
perform the Build stage, and the script arguments are ignored (they are intended to be passed only
to the Generation stage).

Below are the usage examples, where `<build>` is `./build.sh` on Linux and `build.bat` on Windows.

- To make a clean Debug build, delete the build directory (if any), and run the command:
    ```
    <build> -DcustomizationPackage=<customization.zip>
    ```
    The built executables will be placed in `nx_open-build/bin/`, their names may depend on the
    customization package.

- To make a clean Release build with the distribution package and unit test archive, delete the
    build directory (if any), and run the command:
    ```
    <build> -DcustomizationPackage=<customization.zip> -DdeveloperBuild=OFF -DwithDistributions=ON -DwithUnitTestsArchive=ON
    ```
    The built distribution packages and unit test archive will be placed in
    `nx_open-build/distrib/`. To run the unit tests, unpack the unit test archive and run all the
    executables in it either one-by-one, or in parallel.

- To perform an incremental build after some changes, run the `<build>` script without arguments.
    - Note that there is no need to explicitly call the generation stage after adding/deleting
        source files or altering the build system files, because `ninja_tool.py` properly
        handles such cases - the generation stage will be called automatically when needed.

ATTENTION: On Windows, you can use a regular `cmd` console, because `build.bat` calls the
`vcvars64.bat` which comes with the Visual Studio and appropriately sets PATH and other environment
variables to enable using the 64-bit compiler and linker. If you don't use `build.bat`, you may
call `vcvars64.bat` from your console, or use the console available from the Start menu as `VS2019
x64 Native Tools Command Prompt`. Do not use the Visual Studio Command Prompt available from the
Visual Studio main menu, because it sets up the environment for the 32-bit compiler and linker.

For cross-compiling on Linux or MacOS, set the CMake variable `<targetDevice>`: add the argument
`-DtargetDevice=<value>`, where <value> is one of the following:
- `linux_x64`
- `linux_arm64`
- `linux_arm32`
- `macos_x64`
- `macos_arm64`

---------------------------------------------------------------------------------------------------
## Free and Open-Source Software Notices

The Network Optix VMS Open Source Components software incorporates, depends upon, interacts with,
or was developed using a number of free and open-source software components. The full list of such
components can be found at [OPEN SOURCE SOFTWARE DISCLOSURE](https://meta.nxvms.com/content/libraries/).
Please see the linked component websites for additional licensing, dependency, and use information,
as well as the component source code.
