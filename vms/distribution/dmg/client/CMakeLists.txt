set(dmg_file_name ${client_distribution_name}.dmg)
set(update_file_name ${client_update_distribution_name}.zip)

set(configured_files_dir ${CMAKE_CURRENT_BINARY_DIR}/configured_files_dir)

nx_configure_directory(
    ${CMAKE_CURRENT_SOURCE_DIR}/resources.in
    ${configured_files_dir}
    @ONLY
    OUTPUT_FILES_VARIABLE configured_files
)

nx_configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/build_distribution.conf.in
    ${CMAKE_CURRENT_BINARY_DIR}/build_distribution.conf
    @ONLY)
list(APPEND configured_files ${CMAKE_CURRENT_BINARY_DIR}/build_distribution.conf)

nx_configure_directory(
    ${CMAKE_CURRENT_SOURCE_DIR}/resources
    ${configured_files_dir}
    COPYONLY
    OUTPUT_FILES_VARIABLE copied_files
)

nx_configure_directory(
    ${customization_dir}/client-dmg
    ${configured_files_dir}
    COPYONLY
    OUTPUT_FILES_VARIABLE customization_files
)

file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/${launcher.version.file} "${releaseVersion.full}")
nx_store_known_file(${CMAKE_CURRENT_BINARY_DIR}/${launcher.version.file})

nx_copy(${CMAKE_SOURCE_DIR}/vms/libs/common/maven/bin-resources/resources/qt/etc/qt.conf
    DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

add_custom_target(distribution_dmg_client ALL
    DEPENDS desktop_client applauncher
        ${configured_files} ${copied_files} ${customization_files}
        ${customization_dir}/icons/macosx/logo.icns
    COMMENT "Creating ${distribution_output_dir}/${dmg_file_name}"
    BYPRODUCTS
        ${distribution_output_dir}/${dmg_file_name}
        ${distribution_output_dir}/${update_file_name}

    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND ${CMAKE_COMMAND} -E copy
        ${customization_dir}/icons/macosx/logo.icns ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_CURRENT_BINARY_DIR}/dmg-folder
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${configured_files_dir} ${CMAKE_CURRENT_BINARY_DIR}

    # Copying some files and directories for maven compatibility.

    # TODO: Eliminate this copying; adjust python script accordingly.

    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/qml
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/plugins/imageformats
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/plugins/platforms
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/plugins/audio
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/plugins/mediaservice

    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${QT_DIR}/qml ${CMAKE_BINARY_DIR}/bin/qml
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${QT_DIR}/plugins/imageformats ${CMAKE_BINARY_DIR}/bin/imageformats
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${QT_DIR}/plugins/platforms ${CMAKE_BINARY_DIR}/bin/platforms
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${QT_DIR}/plugins/audio ${CMAKE_BINARY_DIR}/bin/audio
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${QT_DIR}/plugins/mediaservice ${CMAKE_BINARY_DIR}/bin/mediaservice
    COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_CURRENT_BINARY_DIR}/${launcher.version.file} ${CMAKE_BINARY_DIR}/bin

    COMMAND /bin/bash ${CMAKE_CURRENT_SOURCE_DIR}/build_distribution.sh

    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/qml
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/plugins/imageformats
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/plugins/platforms
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/plugins/audio
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/plugins/mediaservice
    COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_BINARY_DIR}/bin/${launcher.version.file}

    COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_CURRENT_BINARY_DIR}/${dmg_file_name} ${distribution_output_dir}
    COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_CURRENT_BINARY_DIR}/${update_file_name} ${distribution_output_dir}
)

if(codeSigning)
    prepare_mac_keychain(distribution_dmg_client)
endif()

