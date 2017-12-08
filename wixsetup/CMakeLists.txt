set(project.build.directory "${CMAKE_CURRENT_BINARY_DIR}")
set(bin_resources "${CMAKE_CURRENT_SOURCE_DIR}/maven/bin-resources")
set(filter_resources "${CMAKE_CURRENT_SOURCE_DIR}/maven/filter-resources")
set(ClientHelpSourceDir "${bin_source_dir}/help")
set(VoxSourceDir "${bin_source_dir}/vox")
set(ClientQmlDir "${CMAKE_CURRENT_BINARY_DIR}/clientqml")
set(ClientBgSourceDir "${CMAKE_CURRENT_BINARY_DIR}/Backgrounds")
set(ClientFontsDir "${bin_source_dir}/fonts")
set(target_pack_dir ${CMAKE_CURRENT_BINARY_DIR}/pack)

file(TO_CMAKE_PATH ${wix_directory} wix_directory)
file(TO_CMAKE_PATH ${signtool_directory} signtool_directory)
file(TO_CMAKE_PATH ${certificates_path} certificates_path)
file(TO_CMAKE_PATH ${VC14RedistPath} VC14RedistPath)

ExternalProject_Add(
    customactions
    SOURCE_DIR "${bin_resources}/CustomActions"
    CMAKE_ARGS
        -DCMAKE_INSTALL_PREFIX=${CMAKE_CURRENT_BINARY_DIR}/CustomActions
        -DCONFIG_DIRECTORY=${CMAKE_CURRENT_BINARY_DIR}
        -DROOT_DIR=${CMAKE_SOURCE_DIR}
        -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
        -DwixDirectory=${wix_directory}
)

nx_copy_bin_resources(${bin_resources} "${CMAKE_CURRENT_BINARY_DIR}")
nx_copy_bin_resources("${customization_dir}/wixsetup" "${CMAKE_CURRENT_BINARY_DIR}")
nx_configure_directory(${filter_resources} "${CMAKE_CURRENT_BINARY_DIR}")

file(WRITE ${bin_source_dir}/launcher.version "${releaseVersion.full}")

add_custom_target(wixsetup ALL
    DEPENDS
        desktop_client 
        applauncher 
        traytool 
        mediaserver 
        minilauncher 
        customactions 
        testcamera
        genericrtspplugin 
        mjpg_link 
        generic_multicast_plugin 
        hanwha_metadata_plugin
    COMMENT "Creating installer..."
    # TODO move python to cmake
    COMMAND ${CMAKE_COMMAND} -E copy
        "${bin_source_dir}/minilauncher.exe" "${bin_source_dir}/${minilauncher.binary.name}"
    COMMAND ${CMAKE_COMMAND} -E copy
        "${bin_source_dir}/desktop_client.exe" "${bin_source_dir}/${client.binary.name}"
    COMMAND ${PYTHON_EXECUTABLE} wix-gen.py
    COMMAND ${PYTHON_EXECUTABLE} wix-compile.py
)

file(GLOB_RECURSE ffmpeg_libraries
    "${bin_source_dir}/av*.dll"
    "${bin_source_dir}/lib*.dll"
    "${bin_source_dir}/swscale-*.dll"
    "${bin_source_dir}/swresample-*.dll"
    "${bin_source_dir}/swscale-*.dll"
)

file(GLOB_RECURSE qt_common_libraries
    "${QT_DIR}/bin/icu*.dll"
)

list(APPEND qt_common_libraries
    "${QT_DIR}/bin/Qt5Concurrent.dll"
    "${QT_DIR}/bin/Qt5Core.dll"
    "${QT_DIR}/bin/Qt5Gui.dll"
    "${QT_DIR}/bin/Qt5Multimedia.dll"
    "${QT_DIR}/bin/Qt5Network.dll"
    "${QT_DIR}/bin/Qt5Sql.dll"
    "${QT_DIR}/bin/Qt5Xml.dll"
)

file(GLOB_RECURSE nx_libraries "${bin_source_dir}/nx*.dll")
list(APPEND nx_libraries "${bin_source_dir}/udt.dll")

file(GLOB_RECURSE vc14redist_files
    "${VC14RedistPath}/bin/*"
)

# Qt Plugins
set(qt_plugins platforms imageformats mediaservice)
set(qt_plugin_group_platforms qwindows)
set(qt_plugin_group_imageformats qgif qjpeg qtiff)
set(qt_plugin_group_mediaservice dsengine qtmedia_audioengine)

foreach(qt_plugin_group ${qt_plugins})
    foreach(plugin ${qt_plugin_group_${qt_plugin_group}})
        nx_copy("${QT_DIR}/plugins/${qt_plugin_group}/${plugin}.dll"
            DESTINATION "${target_pack_dir}/${artifact.name.client_update}/${qt_plugin_group}"
            IF_NEWER IF_DIFFERENT)
        if(addQtPdb)
            nx_copy("${QT_DIR}/plugins/${qt_plugin_group}/${plugin}.pdb"
                DESTINATION "${target_pack_dir}/${artifact.name.qt_debug}/${qt_plugin_group}"
                IF_NEWER IF_DIFFERENT)
        endif()
    endforeach()
endforeach()

file(GLOB_RECURSE all_qt_debug_files "${QT_DIR}/bin/Qt5*.pdb")
set(qt_debug_files)
foreach(file ${all_qt_debug_files})
    if(NOT file MATCHES ".*d\\.pdb")
        list(APPEND qt_debug_files ${file})
    endif()
endforeach()

foreach(qt_debug_files_exclude ${qt_debug_files_excludes})
    list(REMOVE_ITEM qt_debug_files "${qt_debug_files_exclude}")
endforeach()

# Processing file lists in order not to pass tons of variables to custom target
nx_configure_directory("${CMAKE_CURRENT_SOURCE_DIR}/packages" "${CMAKE_CURRENT_BINARY_DIR}/packages")

add_custom_target(pack_updates ALL
    DEPENDS wixsetup
    COMMENT "Preparing update and debug packages ..."
    COMMAND
        "${CMAKE_COMMAND}"
        -DPROJECT_SOURCE_DIR=${PROJECT_SOURCE_DIR}
        -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
        -Dtarget_pack_dir=${target_pack_dir}
        -P "${CMAKE_CURRENT_SOURCE_DIR}/pack.cmake"
    COMMAND "pack.bat"
)

