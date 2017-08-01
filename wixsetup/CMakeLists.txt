INCLUDE(CPack)

set(project.build.directory "${CMAKE_CURRENT_BINARY_DIR}")
set(bin_resources "${CMAKE_CURRENT_SOURCE_DIR}/maven/bin-resources")
set(target_bin_path "${CMAKE_BINARY_DIR}/${CMAKE_BUILD_TYPE}/bin")
set(filter_resources "${CMAKE_CURRENT_SOURCE_DIR}/maven/filter-resources")
set(ClientHelpSourceDir "${target_bin_path}/help")
set(ClientVoxSourceDir "${target_bin_path}/vox")
set(ClientQmlDir "${CMAKE_CURRENT_BINARY_DIR}/clientqml")
set(ClientBgSourceDir "${CMAKE_CURRENT_BINARY_DIR}/Backgrounds")
set(ClientFontsDir "${target_bin_path}/fonts")
set(VC14RedistPath "$ENV{environment}/packages/${rdep_target}/vcredist-2015")
#set(NxtoolQuickControlsDir ${libdir}\bin\${build.configuration}\qml\QtQuick\Controls)

ExternalProject_Add(
    customactions
    SOURCE_DIR "${bin_resources}/CustomActions"
    CMAKE_ARGS
        -DCMAKE_INSTALL_PREFIX=${CMAKE_CURRENT_BINARY_DIR}/CustomActions
        -DCONFIG_DIRECTORY=${CMAKE_CURRENT_BINARY_DIR}
        -DROOT_DIR=${CMAKE_SOURCE_DIR}
)

nx_copy_bin_resources(${bin_resources} "${CMAKE_CURRENT_BINARY_DIR}")
nx_copy_bin_resources(${customization_dir}/wixsetup "${CMAKE_CURRENT_BINARY_DIR}")
nx_configure_directory(${filter_resources} "${CMAKE_CURRENT_BINARY_DIR}")

file(WRITE ${bin_source_dir}/launcher.version "${releaseVersion.full}")

file(GLOB_RECURSE ffmpeg_libraries
    ${target_bin_path}/av*.dll
    ${target_bin_path}/lib*.dll
    ${target_bin_path}/swscale-*.dll
    ${target_bin_path}/swresample-*.dll
    ${target_bin_path}/swscale-*.dll
)

file(GLOB_RECURSE qt_common_libraries
    ${QT_DIR}/bin/icu*.dll
)

list(APPEND qt_common_libraries
    ${QT_DIR}/bin/Qt5Concurrent.dll
    ${QT_DIR}/bin/Qt5Core.dll
    ${QT_DIR}/bin/Qt5Gui.dll
    ${QT_DIR}/bin/Qt5Multimedia.dll
    ${QT_DIR}/bin/Qt5Network.dll
    ${QT_DIR}/bin/Qt5Sql.dll
    ${QT_DIR}/bin/Qt5Xml.dll
)

file(GLOB_RECURSE ${target_bin_path}/nx*.dll)
list(APPEND nx_libraries ${target_bin_path}/udt.dll)

file(GLOB_RECURSE vc14redist_files
    ${VC14RedistPath}/bin/*
)

set(client_update_files
    ${ffmpeg_libraries}
    ${qt_common_libraries}
    ${nx_libraries}
    ${vc14redist_files}
    ${CMAKE_CURRENT_BINARY_DIR}/client/update.json
    ${target_bin_path}/${applauncher.binary.name}
    ${target_bin_path}/${client.binary.name}
    ${target_bin_path}/${minilauncher.binary.name}
    ${target_bin_path}/OpenAL32.dll
    ${target_bin_path}/quazip.dll
    ${target_bin_path}/sigar.dll
    ${target_bin_path}/ssleay32.dll
    ${target_bin_path}/udt.dll
    ${QT_DIR}/bin/Qt5LabsTemplates.dll
    ${QT_DIR}/bin/Qt5MultimediaWidgets.dll
    ${QT_DIR}/bin/Qt5MultimediaQuick_p.dll
    ${QT_DIR}/bin/Qt5OpenGL.dll
    ${QT_DIR}/bin/Qt5PrintSupport.dll
    ${QT_DIR}/bin/Qt5Qml.dll
    ${QT_DIR}/bin/Qt5Quick.dll
    ${QT_DIR}/bin/Qt5QuickWidgets.dll
    ${QT_DIR}/bin/Qt5WebChannel.dll
    ${QT_DIR}/bin/Qt5WebKit.dll
    ${QT_DIR}/bin/Qt5WebKitWidgets.dll
    ${QT_DIR}/bin/Qt5Widgets.dll
    ${QT_DIR}/bin/Qt5XmlPatterns.dll
    ${target_bin_path}/fonts
    ${target_bin_path}/vox
    ${CMAKE_CURRENT_BINARY_DIR}/clientqml
    ${ClientHelpSourceDir}
)

foreach(file ${client_update_files})
    nx_copy("${file}" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_BUILD_TYPE}/client_update")
endforeach(file)

#copying plugins
nx_copy("${QT_DIR}/plugins/mediaservice/dsengine.dll" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_BUILD_TYPE}/client_update/mediaservice")
nx_copy("${QT_DIR}/plugins/mediaservice/qtmedia_audioengine.dll" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_BUILD_TYPE}/client_update/mediaservice")
nx_copy("${QT_DIR}/plugins/imageformats/qgif.dll" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_BUILD_TYPE}/client_update/imageformats")
nx_copy("${QT_DIR}/plugins/imageformats/qjpeg.dll" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_BUILD_TYPE}/client_update/imageformats")
nx_copy("${QT_DIR}/plugins/imageformats/qtiff.dll" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_BUILD_TYPE}/client_update/imageformats")
nx_copy("${QT_DIR}/plugins/platforms/qwindows.dll" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_BUILD_TYPE}/client_update/platforms")
nx_copy("${QT_DIR}/plugins/audio/qtaudio_windows.dll" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_BUILD_TYPE}/client_update/audio")

file(GLOB_RECURSE testcamera_files
    ${target_bin_path}/testcamera.exe
    ${ffmpeg_libraries}
    ${qt_common_libraries}
    ${nx_libraries}
)

foreach(file ${testcamera_files})
    nx_copy("${file}" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_BUILD_TYPE}/testcamera")
endforeach(file)

#add_custom_target(wixsetup ALL
#    DEPENDS desktop_client applauncher traytool mediaserver minilauncher customactions
#    COMMENT "Creating installer..."
#    COMMAND ${PYTHON_EXECUTABLE} wix-gen.py
#    COMMAND ${PYTHON_EXECUTABLE} wix-compile.py
#)

# do the copying