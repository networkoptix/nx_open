# Variables used by build-dist.sh and/or .deb contents.
set(build.configuration "") #< Used by build-dist.sh as suffix to lib and bin dirs.
if(arch STREQUAL "x64")
    set(os.arch "amd64")
    set(arch.dir "x86_64-linux-gnu/")
elseif(arch STREQUAL "x86")
    set(os.arch "i386")
    set(arch.dir "i386-linux-gnu/")
endif()
set(target.arch "${os.arch}")
set(common_depends
    "libstdc++6 (>= 4.1.1)"
    "zlib1g (>= 1:1.1.4)"
    "debconf (>= 0.5) | debconf-2.0"
    "python"
    "upstart-job"
    "psmisc"
    "upstart (>= 0.6.5-8)"
    "libsm6"
    "libice6"
    "libaudio2"
    "libogg0"
    "libxfixes3"
    "libxrender1"
    "libfontconfig1"
    "libgl1-mesa-glx"
    "cifs-utils"
)
string(REPLACE ";" ", " common.depends "${common_depends}") #< Make a comma-separated list.
set(arch.depends ", syslinux")
set(license.text "TODO license.text") #< TODO: #dklychkov: Include license text to cmake build.

nx_configure_file(
    ${CMAKE_SOURCE_DIR}/build_environment/maven/filter-resources/version.py
    ${CMAKE_BINARY_DIR}/version.py
)

if(withMediaServer)
    add_subdirectory(mediaserver-deb)
endif()

if(withDesktopClient)
    add_subdirectory(client-deb)
endif()
