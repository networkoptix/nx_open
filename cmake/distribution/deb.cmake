## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

# Variables used by build_distribution.conf files and/or .zip/.deb contents.
if(arch STREQUAL "x64")
    set(targetArch "amd64")
elseif(arch STREQUAL "x86")
    set(targetArch "i386")
elseif(arch STREQUAL "arm64")
    set(targetArch "arm64")
elseif(arch STREQUAL "arm")
    set(targetArch "armhf")
endif()

if(targetDevice STREQUAL "linux_arm32" OR targetDevice STREQUAL "linux_arm64")
    set(variantVersion "") #< ARM distributions should be compatible with any linux flavor.
else()
    set(variantVersion "16.04")
endif()
