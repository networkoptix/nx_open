add_subdirectory(${CMAKE_SOURCE_DIR}/customization customization)
add_subdirectory(${CMAKE_SOURCE_DIR}/customization/${customization} customization/${customization})

set (release.version "${majorVersion}.${minorVersion}.${incrementalVersion}")
set (linux.release.version "${majorVersion}.${minorVersion}.${incrementalVersion}")
set (full.version "${release.version}.${buildNumber}")
set (project.artifactId "${PROJECT_SHORTNAME}")
set (client.mediafolder.name "${product.name} Media")
set (CMAKE_PREFIX_PATH "$ENV{environment}/artifacts/qt/${qt.version}/${platform}/${arch}/${box}")

execute_process(COMMAND hg id -i
                WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                OUTPUT_VARIABLE changeSet)
string(STRIP ${changeSet} changeSet)

set(parsedVersion.majorVersion ${majorVersion})
set(parsedVersion.minorVersion ${minorVersion})
set(parsedVersion.incrementalVersion ${incrementalVersion})
set(root.dir ${CMAKE_SOURCE_DIR})
