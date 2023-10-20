# Building Nx Meta Platform open-source components

// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

---------------------------------------------------------------------------------------------------
## Introduction

This document provides the most actual instructions how to set up the prerequisites, explanation of
the build system internals, and recommendations for using the build and development tools. This
information supplements the brief information about the build process and its prerequisites from
`readme.md` specific to the particular branch of the repository.

ATTENTION: Use this document only from the `master` branch of the `nx_open` repository - it is
kept up-to-date and applicable for building all currently supported branches of the VMS -
`vms_5.0`, `vms_5.1`, and `master`.

---------------------------------------------------------------------------------------------------
## Contribution policy

At this moment, Network Optix is not able to process any pull/merge requests to this repository. It
is likely that a policy for contributions will be developed and offered in the future.

---------------------------------------------------------------------------------------------------
## Build environment

The list of supported and tested platforms and architectures is given in the `readme.md` of
the particular branch of the repository. Building on other OS versions/flavors, or using other
versions of the build tools may work, but may require some adaptation, and the support team will
not be able to provide assistance.

Note that certain components in this repository can be built using more platforms and compilers,
e.g. the Nx Kit library (`artifacts/nx_kit/`) - see its `readme.md`.

To install the pre-packaged CMake, Ninja and Conan, as well as all other Python packages that are
necessary for the build, it is recommended to use the Python's `pip` with the file
**`requirements.txt`** in the root directory of this repository's **`master`** branch - it is
guaranteed to list the proper packages to build any currently supported repository branches.

The VMS build system is configured in such a way that Conan stores the downloaded artifacts in the
`.conan/` directory in the build directory. To avoid re-downloading all the artifacts from the
internet for every clean build, set the environment variable `NX_CONAN_DOWNLOAD_CACHE` to the full
path of a directory that will be used as a transparent download cache; for example, create the
directory `conan_cache/` next to the repository root and the build directories.

### Windows

**Python 3.8+** should be installed and available on `PATH` as `python`. A possible installation
procedure of Python 3.8 could look like the following:
- Disable the Python alias that opens Microsoft Store: go to "Start" ->
    "Manage app execution aliases" and disable all python-related aliases for App Installer
    (`python.exe` and `python3.exe`).
- Download the Python installer from
    [python-3.8.10](https://www.python.org/ftp/python/3.8.10/python-3.8.10-amd64.exe).
- In the installer, select "Add Python 3.8 to PATH". Note that the component "tcl/tk and IDLE"
    is not required.
- It is recommended to activate "Disable path length limit" option at the end of the installer.

**Microsoft Visual Studio** 2022, Community Edition should be installed.
- NOTE: Microsoft Visual Studio 2019 can also be used to build the repository branches `vms_5.0`,
    `vms_5.0_patch` and `vms_5.1`, but its support may be dropped in further branches like
    `vms_5.1_patch`, `vms_6.0` and `master`.

Install **CMake**, **Ninja** and **Conan** via **`pip`**. Note that `pip` must be run with
**Administrator privileges**: find the Command Prompt in the Start menu, right-click it and choose
"Run as administrator" from the menu; then execute the commands:
```
git checkout master #< The master branch contains requirements.txt suitable for all branches.
pip install -r requirements.txt
```
ATTENTION: If you use **Cygwin**, make sure the Cygwin's **`cmake` is not on `PATH`**.

Alternatively to using `pip` with `requirements.txt`, you may install CMake and Ninja manually
(just make sure they appear on `PATH`) or use the ones which come with Microsoft Visual Studio,
and install the following Python packages:
```
pip install conan==<version>
```
Take the `<version>` value from `requirements.txt`.

### Linux

**Python 3.8+** should be installed and available on `PATH` as both `python` and `python3`. A
possible installation procedure of Python 3.8 could look like the following:
- **Ubuntu 18**: The default version of Python in the Ubuntu 18 distribution is `python3.6`
    (Ubuntu 18.04.5 LTS), so it is necessary to upgrade it - some tricks are required. Perform
    the following steps to install the required Python version and configure it:
    ```
    sudo apt update
    sudo apt install python3.8 python3-pip -y
    sudo update-alternatives --install /usr/bin/python3 python3 /usr/bin/python3.8 100
    sudo update-alternatives --install /usr/bin/python python /usr/bin/python3.8 100
    sudo python -m pip install pip --upgrade
    find /usr/lib/python3/dist-packages -name "*apt_*.cpython-3?m*" -exec sh -c \
        "new_name=\$(echo {} | cut -d. -f1); sudo ln -s {} \${new_name}.so" \;
    ```
- **Ubuntu 20**: Make `python` command equivalent to `python3`:
    ```
    sudo update-alternatives --install /usr/bin/python python /usr/bin/python3.8 100
    ```
- If you have Python 2 installed on your machine and you need it for some reason, you can run
    ```
    sudo update-alternatives --install /usr/bin/python python /usr/bin/python2 10
    ```
    and then switch between Python 3 and Python 2 by running
    ```
    sudo update-alternatives --config python
    ```
    and selecting the appropriate version of Python. Same for the older version of Python3 -
    you can add it using
    ```
    sudo update-alternatives --install /usr/bin/python3 python3 /usr/bin/python<your_old_version> 10
    ```

Install **CMake**, **Ninja** and **Conan** via **`pip`** using the command:
```
git checkout master #< The master branch contains requirements.txt suitable for all branches.
pip install -r requirements.txt
```
ATTENTION: `pip` installs the binaries to `~/.local/bin/`, so make sure this directory is on
`PATH`. Normally it is added to `PATH` via a shell profile, but this requires you to re-login after
executing `pip install` in the case this directory did not exist before.

Alternatively to using `pip` with `requirements.txt`, you may install CMake and Ninja manually
(just make sure they appear on `PATH`) and install the following Python packages:
```
pip install conan==<version>
```
Take the `<version>` value from `requirements.txt`.

Install the **build and runtime dependencies** specified in `readme.md` of the particular branch.

NOTE: The C++ compiler is downloaded as a Conan artifact during the CMake Generation stage - the
compilers installed in the Linux system (if any) are not used.

### macOS

**Python 3.8+** should be installed and available on `PATH` as both `python` and `python3`. A
possible installation procedure for Python 3.8 could look like the following:
- Install Homebrew:
  ```
  /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
  ```
- Install and configure Python 3 using `pyenv`:
  ```
  brew install pyenv
  pyenv install 3.8
  pyenv global 3.8
  echo 'eval "$(pyenv init -)"' >>~/.zshrc
  eval "$(pyenv init -)"
  ```

NOTE: Xcode Command Line Tools package is installed automatically together with Homebrew.

Install the **build dependencies** specified in `readme.md` of the particular branch.

Install **CMake**, **Ninja** and **Conan** via **`pip`** using the command:
```
git checkout master #< The master branch contains requirements.txt suitable for all branches.
pip install -r requirements.txt
```

Alternatively to using `pip` with `requirements.txt`, you may install CMake and Ninja manually
(just make sure they appear on `PATH`) and install the following Python packages:
```
pip install conan==<version> dmgbuild==<version>
```
Take `<version>` values from `requirements.txt`.

---------------------------------------------------------------------------------------------------
## Using CMake

We recommend doing out-of-source builds with CMake - the source code folder (VCS folder) remains
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

### Using build.sh/build.bat

When running the scripts with the build directory absent, they create it, perform the CMake
Generation stage (which creates `CMakeCache.txt` in the build directory), and then perform the
Build stage. The same takes place when the build directory exists but `CMakeCache.txt` in it is
absent. When running the scripts with `CMakeCache.txt` in the build directory present, they only
perform the Build stage, and the script arguments are ignored (they are intended to be passed only
to the Generation stage).

ATTENTION: On **Windows**, you can use a regular `cmd` console, because `build.bat` calls the
`vcvars64.bat` which comes with the Visual Studio and properly sets PATH and other environment
variables to enable using the 64-bit compiler and linker. If you don't use `build.bat`, you may
call `vcvars64.bat` from your console, or use the console available from the Start menu as `VS2022
x64 Native Tools Command Prompt`. Do not use the Visual Studio Command Prompt available from the
Visual Studio main menu, because it sets up the environment for the 32-bit compiler and linker.

On Windows, the build can be performed from MinGW (Bit Bash) or **Cygwin** console, either by the
`build.sh` script, or manually calling `cmake`, but the environment variables required by the
Visual Studio toolchain must be set. Calling `vcvars64.bat` from these consoles will not help,
because it will set the variables of the inner `cmd` process, and the values will not be visible
in `bash` after that script finishes. The recommended solution could be capturing the required
variable values from `cmd` and writing a Unix shell script which sets them. A tool which helps
automating this process will likely be provided in the future.

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

Right after opening the directory, Visual Studio will start the CMake Generation stage using the
default build configuration - `Debug (minimal)`. If you need another build configuration, select it
in the Visual Studio main toolbar - the CMake Generation stage will be started immediately, and the
build directory from the previously selected build configuration can be deleted manually.

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
