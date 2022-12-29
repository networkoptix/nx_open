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

### Contribution policy

At this moment, Network Optix is not able to process any pull/merge requests to this repository. It
is likely that a policy for contributions will be developed and offered in the future.

---------------------------------------------------------------------------------------------------
## Build environment

Currently the following target platforms and architectures are supported by this repository:
- Windows x64 (Microsoft Visual Studio).
- Linux x64 (GCC or Clang).
- Linux 64-bit ARM (cross-compiling on Linux x64, GCC or Clang).
- Linux 32-bit ARM (cross-compiling on Linux x64, GCC or Clang).
- MacOS (Xcode with Clang).

NOTE: Certain components in this repository can be built using more platforms and compilers, e.g.
the Nx Kit library (`artifacts/nx_kit/`).

**Windows** host: this guide implies Windows 10 x64 - other versions may require some adaptation.
The build can be triggered from the command line of `cmd`, MinGW (Git Bash), or **Cygwin**.

**Linux** host: this guide implies Ubuntu 18.04 or 20.04 - other flavors/versions may require some
adaptation.

**MacOS** host: for easy installation of the further build prerequisites, install Homebrew:
```
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
brew update
```

Pre-requisites needed to build all of the Components (the details on installing are given below):
- Python 3.8+.
- CMake 3.20+.
- Ninja.
- Conan 1.46.x.
- **Linux:** Certain packages for the build dependencies.
- **Windows:** Microsoft Visual Studio 2022, Community Edition.
    - NOTE: Microsoft Visual Studio 2019 can also be used to build the repository branches
        `vms_5.0`, `vms_5.0_patch` and `vms_5.1`, but its support may be dropped in further
        branches like `vms_5.1_patch` and `master`.
- **MacOS**: Xcode 12.5.

### Python

Python 3.8+ should be installed and available on `PATH` either as `python` or `python3`, and for
**MacOS** - as `python3.<minor>`. In this guide we assume its main command is `python`.

- **Windows**: a Windows-native (non-Cygwin) version of Python should be installed. A Cygwin
    version of Python may be installed additionally, but must appear on `PATH` as a symlink (this
    makes it not visible to Windows-native programs including `cmake`).
- **Linux**:
    - `distutils` and `pip` must be installed explicitly:
        ```
        sudo apt install python3-distutils python3-pip
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
- **MacOS**:
    ```
    brew install python3.9
    python3.9 -m ensurepip
    ```

### CMake, Ninja, and Conan

The file **`requirements.txt`** in the root directory of this repository lists all the Python
packages that are necessary for the build, including pre-packaged CMake, Ninja and Conan. To
install them, check out the **`master` branch** (its `requirements.txt` is guaranteed to list the
proper packages to build any currently supported repository branches), and run the command:
```
pip install -r requirements.txt
```
Here are the hints for the correct installation on different platforms:
- **Windows**:
    - For the correct installation, **`pip`** must be run with **Administrator privileges**: find
      the Command Prompt in the Start menu, right-click it and choose "Run as administrator" from
      the menu.
    - ATTENTION: If you use **Cygwin**, make sure the Cygwin's **`cmake` is not on `PATH`**.
- **Linux**:
    - `pip` installs the binaries to `~/.local/bin/`, so make sure this directory is on `PATH`.

Alternatively to using `requirements.txt`, you may install CMake and Ninja manually (just make sure
they appear on `PATH`) and install the `pyaml` and `conan` Python packages:
```
python -m pip install pyaml conan==1.46.2
```
On **Windows**, CMake and Ninja which come with Microsoft Visual Studio are suitable.

### Conan cache

The VMS build system is configured in such a way that Conan stores the downloaded artifacts in the
`.conan/` directory in the build directory. To avoid re-downloading all the artifacts from the
internet for every clean build, set the environment variable `NX_CONAN_DOWNLOAD_CACHE` to the full
path of a directory that will be used as a transparent download cache; for example, create the
directory `conan_cache/` next to the repository root and the build directories.

### Build tools

- **Windows**:
    Install [Visual Studio, Community Edition](https://visualstudio.microsoft.com/downloads/) and
    make sure that the following components are selected:
    - "The Workload" -> "Desktop development with C++"
    - "Individual components" -> "C++ CMake tools for Windows"

- **Linux**:
    - NOTE: The compiler is downloaded as a Conan artifact during the CMake Generation stage -
        compilers installed in the Linux system (if any) are not used.
    - Install the following build and runtime dependencies:
        ```
        sudo apt install -y \
            chrpath libtinfo5 zlib1g-dev libopenal-dev mesa-common-dev libgl1-mesa-dev \
            libglu1-mesa-dev libldap2-dev libxfixes-dev libxss-dev libgstreamer1.0-0 \
            libgstreamer-plugins-base1.0-0 libxslt1.1 pkg-config autoconf libxcb-xinerama0
        ```

- **MacOS**:
    - Install the latest Xcode from AppStore.
    - Install the following build dependencies:
        ```
        brew install zlib-ng openal-soft mesa mesa-glu mesalib-glw openldap libxfixes \
            libxscrnsaver gstreamer gst-plugins-base libxslt pkgconf
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

If you want to build an Nx-Meta-branded VMS Client, create a Custom Client on the Nx Meta Developer
Portal and download its customization package at https://meta.nxvms.com/developers/custom-clients/.
Otherwise, request a different Powered-by-Nx customization package.

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
    <build> -DcustomizationPackageFile=<customization.zip> -DdeveloperBuild=OFF
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
## Signing executable files

- **Windows**:

    There is an option of signing the built executables (including the distribution file itself)
    with the software publisher certificate. To perform it, a valid certificate file in the PKCS#12
    format is needed.

    Signing is performed by the `signtool.py` script which is a wrapper around native Windows
    `signtool.exe`. To enable signing, the following preparation steps must be done:
    - Save the publisher certificate file somewhere in your file system.
    - Create (preferably outside of the source tree) the configuration file. This file must contain
        the following fields:
        - `file`: the path to the publisher certificate file. It must be either an absolute path or
            a path relative to the directory where the configuration file resides.
        - `password`: the password protecting the publisher certificate file.
        - `timestamp_servers` (optional): a list of the URLs of the trusted timestamping server. If
            this field is present in the configuration file, the signed file will be time-stamped
            using one of the listed servers. If this field is absent, the signed file will not be
            time-stamped.

        The example of a configuration file can be found in
        `open/build_utils/signtool/config/config.yaml`.
    - Add a CMake argument `-DsigntoolConfig=<configuration_file_path>` to the generation stage. If
    this argument is missing, no signing will be performed.

    Also you can sign any file manually by calling `signtool.py` directly:
    `python build_utils/signtool/signtool.py --config <configuration_file> --file <unsigned_file> --output <signed_file>`

    To test the signing procedure, you can use a self-signed certificate. To generate such
    certificate, you can use the file `open/build_utils/signtool/genkey/genkey_signtool.bat`. When
    run, it creates the `certificate.p12` file and a couple of auxillary `*.pem` files in the same
    directory where it is run. We recommend to move these files outside of the source directory to
    maintain the out-of-source build concept.

- **Linux**:

    Signing is not required; no tools or instructions are provided.

- **MacOS**:

    A signing tool suitable for standalone use is being developed and will likely be provided in
    the future. As for now, you can use your regular signing procedure that you involve for your
    other MacOS developments.

---------------------------------------------------------------------------------------------------
## Running VMS Desktop Client

The VMS Desktop Client can be run directly from the build directory, without installing a
distribution package.

After the successful build, the Desktop Client executable is located in `nx_open-build/bin/`; its
name may depend on the customization package.

For **Linux** and **MacOS**, just run the Desktop Client executable.

For **Windows**, before running the Desktop Client executable or any other executable built, run
the following script (generated by Conan during the build) in the console, which properly sets PATH
and some other environment variables:
```
nx_open-build/activate_run.bat
```
To restore the original variable values including PATH, you may run the following script:
```
nx_open-build/deactivate_run.bat
```

### Compatible VMS Server versions

The Desktop Client built from the open-source repository can only connect to a compatible VMS
Server. Because the VMS Server sources are not publicly available, such Server can only be obtained
from any public VMS release, including the official VMS releases, and the regular preview
releases called Nx Meta VMS.

For any given public VMS release, the compatibility is guaranteed only for the Client built from
the same commit as the Server. The particular commit can be identified in the repository by its git
tag. The public release tags look like `vms/4.2/12345_release_all` or `vms/5.0/34567_beta_meta_R2`.

Clients built from further commits in the same branch may retain compatibility with the publicly
released Server for a while, but at some point may lose the compatibility because of some changes
introduced synchronously into the Client and the Server parts of the source code. Thus, it is
recommended to base the Client modification branches from tagged commits corresponding to the
public releases, including Nx Meta VMS releases, and rebase them as soon as next public release
from this branch is available.

Besides having the compatible code, to be able to work together, the Client and Server have to use
the same customization package. If, however, for experimental purposes (not for the production use)
you need to connect to a Server with a different customization, the following option can be added
to `desktop_client.ini`: `developerMode=true`. For the details, see the documentation for the
IniConfig mechanism of the Nx Kit library located at `artifacts/nx_kit/`.

During the Generation stage, the build system tries to determine the compatible Server version
checking the git tags. It searches for the first commit common for the current branch and one of
the "protected" branches (corresponding to stable VMS versions), and checks if it has a "release"
tag of the form "vms/#.#/#####_...". If no such tag is found, the build number is set to 0 and a
warning is produced, otherwise the build number is extracted from the tag. To bypass this
algorithm, pass "-DbuildNumber=<custom_build_number>" to cmake; to get back to it, make a clean
build or pass `-DbuildNumber=AUTO`.

### Automatic VMS updates

The VMS product includes a comprehensive auto-update support, but this feature is turned off for
the open-source Desktop Client, because it would simply re-write a custom-built Desktop Client with
the new version of the Desktop Client built by Nx. Note that the VMS admin still can force such
an automatic update, with the mentioned consequences.

Technically it is possible to specify a custom Update server in the VMS Server settings, deploy a
custom Update server, and prepare the update packages and meta-information according to the VMS
standard, so that the automatic updates will work with a custom VMS built from open source. In the
future, instructions and/or tools for this will likely be provided.

---------------------------------------------------------------------------------------------------
## Building and debugging in Visual Studio

On Windows, besides the command-line way described above, you can use the Visual Studio IDE to
build and debug the Client.

The build configurations (Debug, Release and the like) used by the IDE are defined in the file
`CMakeSettings.json` in the repository root directory. If absent, this file is created during the
Generation stage. It defines a build directory name for each build configuration, and a path to
`cmake.exe` - if you installed CMake manually, write its path to the `cmakeExecutable` parameter.

Open Visual Studio, select "Open a local folder" and choose the repository root folder.
Alternatively, run `devenv.exe` (Visual Studio IDE executable) supplying the repository root
directory as an argument.

Right after opening the directory, Visual Studio will start the CMake generation stage using the
default build configuration - `Debug (minimal)`. If you need another build configuration, select it
in the Visual Studio main toolbar - the CMake generation stage will be started immediately, and the
build directory from the previously selected build configuration can be deleted manually.

When performing the Generation for the first time, it will fail because the required CMake argument
`-DcustomizationPackageFile=...` will not be passed. After the error, open
"Manage Configurations..." in the configuration drop-down box in the Visual Studio toolbar, and add
this argument to the "CMake command arguments:" field in the "Command arguments" section.

If you don't need advanced debugging features, you may choose one of the Release configurations -
the basics of visual debugging like breakpoints and step-by-step execution will work anyway in most
cases, and the build time and disk usage will be noticeably lower than with a Debug configuration.

After successfully finishing the CMake Generation stage, open the "Solution Explorer" side window,
click the "Switch between solutions and available views" toolbar button at the top of this side
window (looking like a document icon with a Visual Studio logo on it), and in the tree below
double-click the "CMake Targets View". The CMake Generation stage will be run again, and when
finished, the tree of CMake targets will appear - watch the "Output" window for the Generation
stage progress.

To build the solution, right-click on "vms Project" in "Solution Explorer" and select "Build All".
Alternatively, build only the required project of the solution, for example, right-click on
"desktop_client" and select "Build".

### Running/debugging

To be able to run and debug the built Desktop Client from the IDE, the file `launch.vs.json` must
be created in the `.vs/` directory which Visual Studio creates in the source (repository root)
directory. The first run of the CMake Generation stage checks if `.vs/` directory already exists
and creates `launch.vs.json` file if it is not the case.

Also it is necessary to specify the actual path to the Qt library binaries in the file
`CMakeSettings.json` in the repository root directory. This is also done automatically by CMake
during the Generation stage.

If all the above steps are performed correctly, you can right-click the required executable, e.g.
"desktop_client", in the "CMake Targets View" of the "Solution Explorer" side window, and select
either "Debug" to run it immediately, or "Set As Startup Item" to allow running it using the green
triangle ("play") icon in the toolbar at the top of the main Visual Studio window.

---------------------------------------------------------------------------------------------------
## Free and Open-Source Software Notices

The Network Optix VMS Open Source Components software incorporates, depends upon, interacts with,
or was developed using a number of free and open-source software components. The full list of such
components can be found at [OPEN SOURCE SOFTWARE DISCLOSURE](https://meta.nxvms.com/content/libraries/).
Please see the linked component websites for additional licensing, dependency, and use information,
as well as the component source code.
