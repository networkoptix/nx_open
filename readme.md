# Nx Meta Platform Open Source Components

// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

---------------------------------------------------------------------------------------------------
## Introduction

This repository `nx_open` contains **Network Optix Meta Platform open source components** - the
source code and specifications which are used to build all Powered-by-Nx products including Nx
Witness Video Management System (VMS).

Currently, the main VMS component which can be built from this repository is the Desktop Client.
Other notable Components which are parts of the Desktop Client, but can be useful independently,
include the Nx Kit library (`artifacts/nx_kit/`) - see its `readme.md` for details.

Most of the source code and other files are licensed under the terms of Mozilla Public License 2.0
(unless specified otherwise in the files) which can be found in the `license_mpl2.md` file in the
`licenses/` directory in the root directory of the repository.

---------------------------------------------------------------------------------------------------
## Build environment

Currently the following platforms and architectures are supported by this repository:
- Windows x64 (Microsoft Visual Studio).
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
- CMake 3.19.0+ (on Windows, comes with Microsoft Visual Studio).
- Ninja (on Windows, comes with Miscosoft Visual Studio).
- Python 3.8+.
- Conan 1.43.x.
- **Linux:**
    - `chrpath`.
    - Certain packages for the build dependencies.
- **Windows:** Microsoft Visual Studio 2022, Community Edition.
    - NOTE: Microsoft Visual Studio 2019 can also be used to build the repository branch `vms_5.0`,
        but its support may be dropped in further branches like `vms_5.0_patch`.
- **MacOS**:
    - XCode 12.5.

### CMake

The Components use CMake as a build system. CMake 3.19.0+ must be installed (the latest stable
version is recommended) and be available on `PATH` as `cmake`.

- **Linux**: either from https://cmake.org/download/, or from the official Kitware APT repository
    for Debian and Ubuntu (https://apt.kitware.com/). It is recommended to completely remove
    (`apt purge -y --auto-remove cmake`) the CMake package from the Main repository before
    installing the one from the Kitware repository.
- **Windows**: CMake which comes with Microsoft Visual Studio is suitable. If desired, a
    standalone installation of CMake can be performed from https://cmake.org/download/; after
    installing, choose "Add CMake to the system PATH for all users" and add the path to `cmake.exe`
    to `CMakeSettings.json` (see the "Generation" section of this guide).
    - ATTENTION: If you use Cygwin, make sure the Cygwin's `cmake` is not on `PATH`.

### Python

Python 3.8+ should be installed and available on `PATH` either as `python` or `python3`.

- **Windows**: a Windows-native (non-Cygwin) version of Python should be installed. A Cygwin
    version of Python may be installed as well, but must appear on `PATH` as a symlink (this makes
    it not visible to Windows-native programs including `cmake`).
- **Linux**:
    - `distutils` must be installed explicitly:
        ```
        sudo apt install python3-distutils
        ```
    - **Ubuntu 18**: Make sure that the right version of Python is installed. First, check the
        currently installed version of `python3`:
        ```
        python3 --version
        ```
        If the version is lower than 3.8, update it:
        ```
        sudo apt install python3.8
        sudo rm /usr/bin/python3
        sudo ln -s /usr/bin/python3.8 /usr/bin/python3
        ```
- Python module `pyaml` should be installed into Python:
    - **Windows:**
        ```
        python -m pip install pyaml
        ```
    - **Linux:**
        ```
        python3 -m pip install pyaml
        ```

### Conan

Conan 1.43.x should be installed. It is recommended to install Conan by Python's `pip`:
- **Windows:**
    ```
    python -m pip install markupsafe==2.0.1 conan==1.43.2
    ```
- **Linux:**
    ```
    python3 -m pip install markupsafe==2.0.1 conan==1.43.2
    ```

NOTE: Conan requires the `markupsafe` package of version no later than 2.0.1.

ATTENTION: `pip` installs package binaries to `~/.local/bin/`, so make sure that this directory is
on `PATH`.

### Build tools

- **Windows**: Install [Visual Studio, Community Edition](https://visualstudio.microsoft.com/downloads/)
    **The Workload** "Desktop development with C++" should be installed. Also make sure that in
    **Individual components**, "C++ CMake tools for Windows" is selected.

- **Linux**:
    - Install the `chrpath` tool.
        ```
        sudo apt install chrpath
        ```
    - Install Ninja build tool using either way:
        - From https://ninja-build.org/; make sure `ninja` is on `PATH`.
        - **Debian/Ubuntu**: Install via apt:
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
        - Pkg-config: `pkg-config`
        - Autoconf: `autoconf`

        A typical command to install all of the above:
        ```
        sudo apt install zlib1g-dev libopenal-dev mesa-common-dev libgl1-mesa-dev \
            libglu1-mesa-dev libldap2-dev libxfixes-dev libxss-dev libgstreamer1.0-0 \
            libgstreamer-plugins-base1.0-0 libxslt1.1 pkg-config autoconf
        ```
    - **Ubuntu 20+**: Install `libtinfo5` library:
        ```
        sudo apt install -y libtinfo5
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

ATTENTION: If the generation fails for any reason, remove `CMakeCache.txt` manually before the next
attempt of running the build script.

Below are the usage examples, where `<build>` is `./build.sh` on Linux and `build.bat` on Windows.

- To make a clean Debug build, delete the build directory (if any), and run the command:
    ```
    <build> -DcustomizationPackageFile=<customization.zip>
    ```
    The built executables will be placed in `nx_open-build/bin/`.

- To make a clean Release build with the distribution package and unit test archive, delete the
    build directory (if any), and run the command:
    ```
    <build> -DcustomizationPackageFile=<customization.zip> -DdeveloperBuild=OFF -DwithDistributions=ON -DwithUnitTestsArchive=ON
    ```
    The built distribution packages and unit test archive will be placed in
    `nx_open-build/distrib/`. To run the unit tests, unpack the unit test archive and run all the
    executables in it either one-by-one, or in parallel.

- To perform an incremental build after some changes, run the `<build>` script without arguments.
    - Note that there is no need to explicitly call the generation stage after adding/deleting
        source files or altering the build system files, because `ninja_tool.py` properly
        handles such cases - the generation stage will be called automatically when needed.

ATTENTION: On **Windows**, you can use a regular `cmd` console, because `build.bat` calls the
`vcvars64.bat` which comes with the Visual Studio and properly sets PATH and other environment
variables to enable using the 64-bit compiler and linker. If you don't use `build.bat`, you may
call `vcvars64.bat` from your console, or use the console available from the Start menu as `VS2022
x64 Native Tools Command Prompt`. Do not use the Visual Studio Command Prompt available from the
Visual Studio main menu, because it sets up the environment for the 32-bit compiler and linker.

For **cross-compiling** on Linux or MacOS, set the CMake variable `<targetDevice>`: add the
argument `-DtargetDevice=<value>`, where <value> is one of the following:
- `linux_x64`
- `linux_arm64`
- `linux_arm32`
- `macos_x64`
- `macos_arm64`

---------------------------------------------------------------------------------------------------
## Running VMS Desktop Client

The VMS Desktop Client can be run direclty from the build directory, without installing a
distribution package.

After the successful build, the Desktop Client executable is located in `nx_open-build/bin/`; its
names may depend on the customization package.

For **Linux** and **MacOS**, just run the Desktop Client executable.

For **Windows**, before running the Desktop Client executable, add the directory of the Qt library
to the `PATH`:
```
set PATH=%PATH%;<qt_library_directory>/bin
```
Note the `/bin` ending appended to the Qt library directory. The Qt library directory can be found
in the generated file `nx_open-build/conan_paths.cmake, in the line looking like:
```
set(CONAN_QT_ROOT ...)
```

### Compatible VMS Server versions

The Desktop Client built from the open-source repository can only connect to a compatible VMS
Server. Because the VMS Server sources are not publicly available, such Server can only be obtained
from any public VMS release, including the official VMS releases, and the regular preview
releases called MetaVMS.

For any given public VMS release, the compatibility is guaranteed only for the Client built from
the same commit as the Server. The particular commit can be identified in the repository by its git
tag. The public release tags look like `vms/4.2/12345_release_all` or `vms/5.0/34567_beta_meta_R2`.

Clients built from further commits in the same branch may retain compatibility with the publicly
released Server for a while, but at some point may lose the compatibility because of some changes
introduced synchronously into the Client and the Server parts of the source code. Thus, it is
recommended to base the Client modification branches from tagged commits corresponding to the
public releases, including MetaVMS releases, and rebase them as soon as next public release from
this branch is available.

Besides having the compatible code, to be able to work together, the Client and Server have to use
the same customization package. If, however, for experimental purposes (not for the production use)
you need to connect to a Server with a different customization, the following option can be added
to `desktop_client.ini`: `developerMode=true`. For the details, see the documentation for the
IniConfig mechanism of the Nx Kit library located at `artifacts/nx_kit/`.

---------------------------------------------------------------------------------------------------
## Free and Open-Source Software Notices

The Network Optix VMS Open Source Components software incorporates, depends upon, interacts with,
or was developed using a number of free and open-source software components. The full list of such
components can be found at [OPEN SOURCE SOFTWARE DISCLOSURE](https://meta.nxvms.com/content/libraries/).
Please see the linked component websites for additional licensing, dependency, and use information,
as well as the component source code.
