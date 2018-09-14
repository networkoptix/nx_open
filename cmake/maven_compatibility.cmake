set(release.version ${releaseVersion})
set(parsedVersion.majorVersion ${PROJECT_VERSION_MAJOR})
set(parsedVersion.minorVersion ${PROJECT_VERSION_MINOR})
set(parsedVersion.incrementalVersion ${PROJECT_VERSION_PATCH})

set(ffmpeg.version ${ffmpeg_version})
set(boost.version ${boost_version})
set(sigar.version ${sigar_version})

set(build.configuration ${CMAKE_BUILD_TYPE})
set(environment.dir "$ENV{environment}")
set(qt.dir ${QT_DIR})
set(qt.version ${qt_version})
set(customization.dir ${customization_dir})

set(rdep.target ${rdep_target})
set(packages.dir ${PACKAGES_DIR})
set(libdir ${CMAKE_CURRENT_BINARY_DIR})
set(ClientVoxSourceDir "${CMAKE_CURRENT_BINARY_DIR}/bin/vox")

set(installer.target.dir ${build.configuration})
set(bin_source_dir ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
