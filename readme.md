# Nx Meta VMP Open Source Components

// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

---------------------------------------------------------------------------------------------------
## Introduction

This repository `nx_open` contains **Network Optix Meta Video Management Platform open source
components** - the source code and specifications which are part of Nx Meta Video Management
Platform (VMP) which in turn is used to build all Powered-by-Nx products, including Nx Witness
Video Management System (VMS).

Currently, the main VMS component which can be build from this repository is the Desktop Client.
Other notable Components which are parts of the Desktop Client but can be useful independently,
include the Nx Kit library (`artifacts/nx_kit/`) - see its `readme.md` for details.

Most of the source code and other files are licensed under the terms of Mozilla Public License 2.0
(unless specified otherwise in the files) which can be found in the license_mpl2.md file in the
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
    build can be triggered from the command line of `cmd`, MinGW (Git Bash), or **Cygwin**
    (recommended).
- **Linux**: this guide implies Debian 10 or Ubuntu (18.04 or 20.04) - other flavors/versions may
    require some adaptation.

Here we describe the pre-requisites which are needed to build all of the Components.

NOTE: Certain Components in this repository can be built using more platforms and compilers, e.g.
the Nx Kit library (`artifacts/nx_kit/`).

Pre-requisites per platform (the details are given below):
- CMake 3.19.0+ (on Windows, comes with Microsoft Visual Studio 2019).
- Java JDK 8.
- Python 3.8+.
- Conan 1.42.x.
- For Linux:
    - Ninja.
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
    standalone installation of CMake can be performed from https://cmake.org/download; after
    installing, choose "Add CMake to the system PATH for all users" and add the path to `cmake.exe`
    to `CMakeSettings.json` (see the "Generation" section of this guide).
    - ATTENTION: If you use Cygwin, make sure the Cygwin's `cmake` is not on `PATH`.

### Java
    
Java JDK 8 should be installed, and `java` should be available on `PATH`.

- **Windows**: install `java-1.8.0-openjdk` from https://github.com/ojdkbuild/ojdkbuild/.
- **Linux**:
    - On Debian 10, add "stretch" (oldstable) to your apt source list. Then run:
        ```
        sudo apt-get update
        sudo apt install openjdk-8-jdk
        sudo update-alternatives --config java # and choose java-8
        ```
    - On apt-based systems where `openjdk-8` is already available, run:
        ```
        sudo apt install openjdk-8-jdk
        ```
- For macOS:
    ```
    brew tap adoptopenjdk/openjdk
    brew cask install adoptopenjdk8
    ```

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
    **The Workload** "Desktop development with C++" should be installed. Also make sure that the
    **Individual components** "C++ CMake tools for Windows" and "MSVC v140 - VS 2015 C++ build
    tools (v14.00)" are selected.

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
intact, and all build results go into a dedicated folder. E.g. sources are in `nx_open/`, build 
goes to `nx_open-build`. 

With CMake, building is done in two phases:
- Generation phase: performed by cmake).
- Build phase: performed by a build tool chosen at the generation phase - we use `ninja` for both
    Linux and Windows platforms.

After the generation is performed once, further building attempts are performed invoking the chosen
build tool – no need to manually invoke the generation phase again, because the generated rules for
the build tool include calling CMake generation phase when necessary, e.g. when the CMake or source
files change. Thus, generally, invoking the Generation phase should be done only once - after the 
initial cloning of the repository.

### Under the hood

During the build phase the following things happen:
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
    arises, `ninja` will run `cmake` to perform the generation phase and then return to the build
    phase again.
    
---------------------------------------------------------------------------------------------------
## Building VMS Desktop Client

To build the Client, besides cloning this repository, you must obtain a Customization Package -
it is a collection of texts and graphics used in the GUI; it defines the branding of the VMS. The
Customization Package comes as a zip file.

If you want to build an Nx-Meta-branded VMS Client, download the Nx Meta customization package from
https://meta.nxvms.com. Otherwise, download a different Powered-by-Nx customization package using
the corresponding Developer account.

To perform the CMake Configuration phase, all the necessary commands are written in the script
`configure.sh` located in the repository root. On Windows, we recommend to use Cygwin to run it, or
to study the script to find out the details about the commands.

Navigate to the `nx_open/` directory (the repository root), and run the command:
```
[TARGET_DEVICE=<targetDevice>] ./configure.sh -DcustomizationPackageFile=<customization.zip> [<options>]
```

Here `<targetDevice>` is one of the following:
- `windows_x64`
- `linux_x64`
- `linux_arm64`
- `linux_arm32`
- `macos_x64`
- `macos_arm64`

If no `<tagetDevice>` is specified it is autodetected basing on the system information.

Here `<options>` are any additional CMake generation phase arguments - will be passed to `cmake`.

After running the script, a build directory is created as a sibling to the repository root
directory: `nx_open-build/`.

To perform the build, run the command:
```
cmake --build nx_open-build
```

---------------------------------------------------------------------------------------------------
## Free and Open-Source Software Notices

The Network Optix VMS Open Source Components software incorporates, depends upon, interacts with,
or was developed using a number of free and open-source software components. The full list of such
components can be found at [OPEN SOURCE SOFTWARE DISCLOSURE](https://meta.nxvms.com/content/libraries/).
Please see the linked component websites for additional licensing, dependency, and use information,
as well as the component source code.
