set(dmg_file_name ${client_distribution_name}.dmg)
set(update_file_name ${client_update_distribution_name}.zip)

set(libdir ${CMAKE_BINARY_DIR})
set(build.configuration)
set(ClientHelpSourceDir ${help_directory}/bin/help)

set(filtered_files_dir ${CMAKE_CURRENT_BINARY_DIR}/filtered_files_dir)

nx_configure_directory(
    ${CMAKE_CURRENT_SOURCE_DIR}/maven/filter-resources
    ${filtered_files_dir}
    @ONLY
    OUTPUT_FILES_VARIABLE filtered_files
)

nx_configure_directory(
    ${CMAKE_CURRENT_SOURCE_DIR}/maven/bin-resources
    ${filtered_files_dir}
    COPYONLY
    OUTPUT_FILES_VARIABLE copied_files
)

nx_configure_directory(
    ${customization_dir}/client-dmg
    ${filtered_files_dir}
    COPYONLY
    OUTPUT_FILES_VARIABLE customization_files
)

file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/${launcher.version.file} "${releaseVersion.full}")
nx_store_known_file(${CMAKE_CURRENT_BINARY_DIR}/${launcher.version.file})

add_custom_target(desktop_client_dmg ALL
    DEPENDS desktop_client applauncher
        ${filtered_files} ${copied_files} ${customization_files}
        ${customization_dir}/icons/macosx/logo.icns
    COMMENT "Creating ${distribution_output_dir}/${dmg_file_name}"
    BYPRODUCTS
        ${distribution_output_dir}/${dmg_file_name}
        ${distribution_output_dir}/${update_file_name}

    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND ${CMAKE_COMMAND} -E copy
        ${customization_dir}/icons/macosx/logo.icns ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_CURRENT_BINARY_DIR}/dmg-folder
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${filtered_files_dir} ${CMAKE_CURRENT_BINARY_DIR}

    # Copying some files and directories for maven compatibility.
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${libdir}/qml
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${libdir}/plugins/imageformats
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${libdir}/plugins/platforms
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${libdir}/plugins/audio
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${libdir}/plugins/mediaservice

    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${QT_DIR}/qml ${libdir}/bin/qml
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${QT_DIR}/plugins/imageformats ${libdir}/bin/imageformats
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${QT_DIR}/plugins/platforms ${libdir}/bin/platforms
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${QT_DIR}/plugins/audio ${libdir}/bin/audio
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${QT_DIR}/plugins/mediaservice ${libdir}/bin/mediaservice
    COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_CURRENT_BINARY_DIR}/${launcher.version.file} ${libdir}/bin

    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/build_dmg.sh

    COMMAND ${CMAKE_COMMAND} -E remove_directory ${libdir}/qml
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${libdir}/plugins/imageformats
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${libdir}/plugins/platforms
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${libdir}/plugins/audio
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${libdir}/plugins/mediaservice
    COMMAND ${CMAKE_COMMAND} -E remove ${libdir}/bin/${launcher.version.file}

    COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_CURRENT_BINARY_DIR}/${dmg_file_name} ${distribution_output_dir}
    COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_CURRENT_BINARY_DIR}/${update_file_name} ${distribution_output_dir}
)

if(NOT useLoginKeychain AND NOT ${mac.skip.sign})
    nx_find_first_matching_file(certificate "${certificates_path}/macosx/*.p12")
    if(NOT certificate)
        message(FATAL_ERROR "Cannot find any certificates in ${certificates_path}/macosx")
    endif()

    add_custom_target(prepare_keychain
        ${CMAKE_SOURCE_DIR}/build_utils/shell/prepare_build_keychain.sh
            --keychain ${codeSigningKeychainName}
            --keychain-password ${codeSigningKeychainPassword}
            --certificate ${certificate}
            --certificate-password ${mac_certificate_file_password}
    )

    add_dependencies(desktop_client_dmg prepare_keychain)
endif()

