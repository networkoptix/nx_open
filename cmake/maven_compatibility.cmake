set(release.version ${releaseVersion})
set(parsedVersion.majorVersion ${PROJECT_VERSION_MAJOR})
set(parsedVersion.minorVersion ${PROJECT_VERSION_MINOR})
set(parsedVersion.incrementalVersion ${PROJECT_VERSION_PATCH})

set(ffmpeg.version ${ffmpeg_version})
set(boost.version ${boost_version})
set(sigar.version ${sigar_version})

set(qt.dir ${QT_DIR})
set(root.dir ${CMAKE_SOURCE_DIR})
set(customization.dir ${customization_dir})

set(artifact.name.product "${installer.name}")
set(artifact.name.version "${release.version}.${buildNumber}")
set(artifact.name.prefix "${artifact.name.product}")
set(artifact.name.platform "${box}")

if(NOT "${cloudGroup}" STREQUAL "prod")
    set(artifact.name.cloud.suffix "-${cloudGroup}")
endif()

if(beta)
    set(artifact.name.beta.suffix "-beta")
endif()

string(CONCAT artifact.name.suffix
    "${artifact.name.version}-${artifact.name.platform}"
    "${artifact.name.beta.suffix}"
    "${artifact.name.cloud.suffix}")

set(artifact.name.server "${artifact.name.prefix}-server-${artifact.name.suffix}")
set(artifact.name.server_update "${artifact.name.prefix}-server_update-${artifact.name.suffix}")

set(rdep.target ${rdep_target})
set(packages.dir ${PACKAGES_DIR})
set(libdir ${CMAKE_CURRENT_BINARY_DIR})
set(ClientVoxSourceDir "${CMAKE_CURRENT_BINARY_DIR}/bin/vox")
