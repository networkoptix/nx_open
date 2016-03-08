add_subdirectory(${CMAKE_SOURCE_DIR}/customization customization)
add_subdirectory(${CMAKE_SOURCE_DIR}/customization/${customization} customization/${customization})

set (release.version "${majorVersion}.${minorVersion}.${incrementalVersion}")
set (linux.release.version "${majorVersion}.${minorVersion}.${incrementalVersion}")
set (full.version "${release.version}.${buildNumber}")
set (project.artifactId "${PROJECT_SHORTNAME}")
set (client.mediafolder.name "${product.name} Media")
set (CMAKE_PREFIX_PATH "$ENV{environment}/artifacts/qt/${qt.version}/${platform}/${arch}/${box}")

execute_process(COMMAND hg id -i OUTPUT_VARIABLE changeSet)
string(REPLACE "\n" "" changeSet ${changeSet})