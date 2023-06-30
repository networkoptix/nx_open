## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

set(release.version ${releaseVersion})
set(parsedVersion.majorVersion ${PROJECT_VERSION_MAJOR})
set(parsedVersion.minorVersion ${PROJECT_VERSION_MINOR})
set(parsedVersion.incrementalVersion ${PROJECT_VERSION_PATCH})

set(ffmpeg.version ${ffmpeg_version})
set(boost.version ${Boost_VERSION})

set(build.configuration ${CMAKE_BUILD_TYPE})
set(environment.dir "$ENV{environment}")
set(qt.dir ${QT_DIR})
set(qt.version ${qt_version})
set(customization.dir ${customization_dir})

set(libdir ${CMAKE_CURRENT_BINARY_DIR})
set(ClientVoxSourceDir "${CMAKE_CURRENT_BINARY_DIR}/bin/vox")

set(installer.target.dir ${build.configuration})
set(bin_source_dir ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
