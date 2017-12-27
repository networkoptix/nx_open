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
