set(release.version ${releaseVersion})
set(parsedVersion.majorVersion ${PROJECT_VERSION_MAJOR})
set(parsedVersion.minorVersion ${PROJECT_VERSION_MINOR})
set(parsedVersion.incrementalVersion ${PROJECT_VERSION_PATCH})

set(ffmpeg.version ${ffmpeg_version})
set(boost.version ${boost_version})
set(sigar.version ${sigar_version})
string(TOLOWER ${CMAKE_BUILD_TYPE} build.configuration)
set(root.dir "${PROJECT_SOURCE_DIR}")
set(environment.dir "$ENV{environment}")
#TODO Find out how to determine the desired --config
set(bin_source_dir "${CMAKE_BINARY_DIR}/Release/bin")
if(WINDOWS)
    set(product.title "${company.name} ${display.product.name} Setup v.${release.version}")
endif()