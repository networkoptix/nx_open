add_subdirectory(${CMAKE_SOURCE_DIR}/customization customization)
add_subdirectory(${CMAKE_SOURCE_DIR}/customization/${customization} customization/${customization})

if (NOT PROJECT_DISPLAY_LONGNAME)
  set (PROJECT_DISPLAY_LONGNAME "${PROJECT_LONGNAME}")
endif()
set (release.version "${majorVersion}.${minorVersion}.${incrementalVersion}")
set (linux.release.version "${majorVersion}.${minorVersion}.${incrementalVersion}")
set (full.version "${release.version}.${buildNumber}")
set (project.artifactId "${PROJECT_SHORTNAME}")
set (client.mediafolder.name "${product.name} Media")
set (CMAKE_PREFIX_PATH "$ENV{environment}/artifacts/qt/${qt.version}/${platform}/${arch}/${box}")
set (openal.version "1.16")
set (directx.version "JUN2010")
set (festival.version "2.1")
set (ffmpeg.version "0.11.4")
set (gmock.version "1.7.0")
set (gtest.version "1.7.0")
set (mfx.version "2012-R2")
set (onvif.version "2.1.2-io")
set (openssl.version "1.0.2e")
set (quazip.version "0.6.2")
set (sigar.version "1.7")
set (vmax_integration.version "2.1")
execute_process(COMMAND hg id -i OUTPUT_VARIABLE changeSet)
string(REPLACE "\n" "" changeSet ${changeSet})