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

# Read license.txt and properly format it to be included into deb templates file:
#  - All lines but the first one must start with a space.
#  - Empty lines must contain a single period (preceeded by a space too).
file(READ ${customization_dir}/license.txt license.text)
string(REGEX REPLACE "\n\n" "\n.\n" license.text "${license.text}")
string(REGEX REPLACE "\n" "\n " license.text "${license.text}")

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
