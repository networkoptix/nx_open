# Nx Meta Platform open-source components

// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

---------------------------------------------------------------------------------------------------
## Introduction

This repository `nx_open` contains **Network Optix Meta Platform open-source components** - the
source code and specifications which are used to build all Powered-by-Nx products including Nx
Witness Video Management System (VMS).

Currently, the main VMS component which can be built from this repository is the Desktop Client.
Other notable components which are parts of the Desktop Client, but can be useful independently,
include the Nx Kit library (`artifacts/nx_kit/`) - see its `readme.md` for details.

Most of the source code and other files are licensed under the terms of Mozilla Public License 2.0
(unless specified otherwise in the files) which can be found in the `license_mpl2.md` file in the
`licenses/` directory in the root directory of the repository.

ATTENTION: This document provides only brief information about the build process and its
prerequisites, specific to the current branch. For the most actual instructions how to set up the
prerequisites, explanation of the build system internals, and recommendations for using the build
and development tools, refer to the following document in the `master` branch of this repository:
[build.md](https://github.com/networkoptix/nx_open/blob/master/build.md)

---------------------------------------------------------------------------------------------------
## Free and open-source software notices

The "Network Optix Meta Platform open-source components" software incorporates, depends upon,
interacts with, or was developed using a number of free and open-source software components. The
full list of such components can be found at [OPEN SOURCE SOFTWARE DISCLOSURE](https://meta.nxvms.com/content/libraries/).
Please see the linked component websites for additional licensing, dependency, and use information,
as well as the component source code.

---------------------------------------------------------------------------------------------------
## Build environment

Supported target platforms and architectures:
- Windows 10 x64 (Microsoft Visual Studio).
- Linux Ubuntu 18.04, 20.04, 22.04 (GCC or Clang) x64, ARM 32/64 (cross-compiling on Linux x64).
- macOS Monterey 12.6.3 (Xcode with Clang) x64, Apple M1/M2.

Build prerequisites:
- **Python 3.8+** - should be available on `PATH` as `python`, and for macOS and Ubuntu also as
    `python3`.
- **Pip** - should be available on `PATH` as `pip` and be installed for the Python interpreter used
    by the build.
- **CMake, Ninja, Conan** - recommended to be installed via **`pip`** from `requirements.txt` of
    the `master` branch; you may see the required versions in this file.
- **Linux:** Install the **build and runtime dependencies** via CMake by specifying the `cmake`
    command-line argument `-DinstallSystemRequirements=ON` at the Generation stage (may ask for a
    `sudo` password).
    - NOTE: The compiler is downloaded as a Conan artifact during the CMake Generation stage -
        compilers installed in the Linux system (if any) are not used.
- **Windows:** Microsoft Visual Studio 2022,
    [Community Edition](https://visualstudio.microsoft.com/downloads/); select the components:
    - "The Workload" -> "Desktop development with C++"
    - "Individual components" -> "C++ CMake tools for Windows"
- **macOS:** **Xcode Command Line Tools 14.2+**; also install the following **build dependencies:**
    - For Apple M1/M2, install Rosetta 2:
    ```
    /usr/sbin/softwareupdate --install-rosetta --agree-to-license
    ```

---------------------------------------------------------------------------------------------------
## Building VMS Desktop Client

The Client uses in its GUI a collection of texts and graphics called a Customization Package; it
defines the branding of the VMS. The Customization Package comes as a zip file. A default one is
taken from Conan - the Client will be branded as Nx Meta and will show placeholders for such traits
as the company name, web site and End-User License Agreement text. If you want to define these
traits, create a "Custom Client" entity on the Nx Meta Developer Portal and download the generated
Customization Package zip at https://meta.nxvms.com/developers/custom-clients/. Customization
Packages with branding other than Nx Meta can be available there as well.

All the commands necessary to perform the CMake Configuration and Build stages are written in the
scripts `build.sh` (for Linux and macOS) and `build.bat` (for Windows) located in the repository
root. Please treat these scripts as a quick start aid, study their source, and feel free to use
your favorite C++ development workflow instead.

The scripts create/use the build directory as a sibling to the repository root directory, having
added the `-build` suffix. Here we assume the repository root is `nx_open/`, so the build directory
will be `nx_open-build/`.

ATTENTION: If the generation fails for any reason, remove `CMakeCache.txt` manually before the next
attempt of running the build script.

Below are the usage examples, where `<build>` is `./build.sh` for Linux and macOS, and `build.bat`
for Windows.

- To make a clean Debug build, delete the build directory (if any), and run the command:
    ```
    <build>
    ```
    The built executables will be placed in `nx_open-build/bin/`.

- To make a clean Release build with the distribution package and unit test archive, delete the
    build directory (if any), and run the command:
    ```
    <build> -DdeveloperBuild=OFF
    ```
    The built distribution packages and unit test archive will be placed in
    `nx_open-build/distrib/`. To run the unit tests, unpack the unit test archive and run all the
    executables in it either one-by-one, or in parallel.

- To use the obtained Customization Package rather than the default one coming from Conan
    (Nx-Meta-branded with placeholders), add the following arguments to the `<build>` script:
    ```
    -DcustomizationPackageFile=<customization.zip>
    ```
    NOTE: The value in the `"id":` field of `description.json` inside the specified zip file must
    match the one in the Server in order to be able to connect to it.

- To perform an incremental build after some changes, run the `<build>` script without arguments.
    - Note that there is no need to explicitly call the Generation stage after adding/deleting
        source files or altering the build system files, because `ninja_tool.py` properly
        handles such cases - the Generation stage will be called automatically when needed.

For **cross-compiling** on Linux or macOS, set the CMake variable `<targetDevice>`: add the
argument `-DtargetDevice=<value>`, where <value> is one of the following:
- `linux_x64`
- `linux_arm64`
- `linux_arm32`
- `macos_x64`
- `macos_arm64`

Building and debugging in Visual Studio IDE is also supported: run the Generation stage from the
command line, it will create `CMakeSettings.json` and `launch.vs.json`, then open the project.

It is recommended to set the environment variable `NX_CONAN_DOWNLOAD_CACHE` to the full path of a
directory that will be used to avoid re-downloading all the artifacts from the internet for every
clean build; for example, create the directory `conan_cache/` next to the repository root and the
build directories.

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
        `build_utils/signtool/config/config.yaml`.
    - Add a CMake argument `-DsigntoolConfig=<configuration_file_path>` to the Generation stage. If
    this argument is missing, no signing will be performed.

    Also you can sign any file manually by calling `signtool.py` directly:
    `python build_utils/signtool/signtool.py --config <configuration_file> --file <unsigned_file> --output <signed_file>`

    To test the signing procedure, you can use a self-signed certificate. To generate such
    certificate, you can use the file `build_utils/signtool/genkey/genkey_signtool.bat`. When
    run, it creates the `certificate.p12` file and a couple of auxillary `*.pem` files in the same
    directory where it is run. We recommend to move these files outside of the source directory to
    maintain the out-of-source build concept.

- **Linux**:

    Signing is not required; no tools or instructions are provided.

- **macOS**:

    A signing tool suitable for standalone use is being developed and will likely be provided in
    the future. As for now, you can use your regular signing procedure that you involve for your
    other macOS developments.

---------------------------------------------------------------------------------------------------
## Running VMS Desktop Client

The VMS Desktop Client can be run directly from the build directory, without installing a
distribution package.

After the successful build, the Desktop Client executable is located in `nx_open-build/bin/`; its
name may depend on the Customization Package.

For **Linux** and **macOS**, just run the Desktop Client executable.

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

ATTENTION: Besides having the compatible code, to be able to work together, the Client and the
Server have to use Customization Packages with the same `<customization_id>` value.

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
