set(depends desktop_client)

# Files copied to .deb by build-dist.sh.

file(WRITE "${CMAKE_BINARY_DIR}/bin/${launcher.version.file}" "${releaseVersion.full}")

file(COPY "${CMAKE_SOURCE_DIR}/build_environment/maven/bin-resources/backgrounds"
    DESTINATION "${CMAKE_BINARY_DIR}/")

set(plugins
    imageformats
    mediaservice
    audio
    xcbglintegrations
    platforminputcontexts
    platforms
)
foreach(plugin ${plugins})
    file(COPY "${QT_DIR}/plugins/${plugin}" DESTINATION "${CMAKE_BINARY_DIR}/bin/")
endforeach()

file(COPY "${QT_DIR}/qml" DESTINATION "${CMAKE_BINARY_DIR}/bin/")

file(COPY "${CMAKE_SOURCE_DIR}/common/maven/bin-resources/resources/qt/etc/qt.conf"
    DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")

# Variables used by build-dist.sh.
set(ClientHelpSourceDir "${help_directory}/bin/help")
set(common_depends
    "libstdc++6 (>= 4.1.1)"
    "zlib1g (>= 1:1.1.4)"
    "debconf (>= 0.5) | debconf-2.0"
    "upstart-job"
    "psmisc"
    "upstart (>= 0.6.5-8)"
    "libgstreamer-plugins-base0.10-0"
    "libxi6"
    "libxrender1"
    "libxcomposite1"
    "libxt6"
    "libfontconfig1"
    "libsm6"
    "libice6"
    "libaudio2"
)
string(REPLACE ";" ", " common.depends "${common_depends}") #< Make a comma-separated list.

# Variables which may depend on the current project and are used by build-dist.sh.
set(project.build.directory ${CMAKE_CURRENT_BINARY_DIR})

nx_configure_directory(
    ${CMAKE_CURRENT_SOURCE_DIR}/maven/filter-resources
    ${CMAKE_CURRENT_BINARY_DIR}
    @ONLY
    OUTPUT_FILES_VARIABLE filtered)

set(zip_file ${distribution_output_dir}/${artifact.name.client_update}.zip)
set(deb_file ${distribution_output_dir}/${artifact.name.client}.deb)
set(installer_output_list ${zip_file} ${deb_file})

set(script ${CMAKE_CURRENT_BINARY_DIR}/build-dist.sh)
add_custom_command(
    OUTPUT ${installer_output_list}
    COMMAND ${script}
    DEPENDS ${script} ${depends} ${filtered}
    COMMENT "Creating desktop client .deb package"
)

add_custom_target(desktop-client-deb ALL
    DEPENDS ${installer_output_list}
)
