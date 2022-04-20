## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

include(utils)
include(wix_utils)

set(desktop_client_qml_root
    "${open_source_root}/vms/client/nx_vms_client_desktop/qml/")

execute_process(
    WORKING_DIRECTORY ${open_source_root}/build_utils
    COMMAND ${PYTHON_EXECUTABLE} qmldeploy.py
        "--qml-root" ${desktop_client_qml_root}
        "--qt-root" ${qt.dir}
        "--qt-host-root" ${QT_HOST_PATH}
        "--output" ${client_qml_directory}
        "--prefer-compiled"
        "--skip-styles"
        "--clean"
)

file(GLOB_RECURSE client_qml_files CONFIGURE_DEPENDS "${client_qml_directory}/*")
nx_store_known_files(${client_qml_files})

nx_wix_heat(${CMAKE_CURRENT_BINARY_DIR}/client_qml.wixobj
    SOURCE_DIR ${client_qml_directory}
    TARGET_DIR_ALIAS QmlDirectoryReference
    COMPONENT_GROUP QmlComponentGroup)

nx_wix_candle(client_qml_module)
nx_wix_add_light_target(distribution_wix_client_qml_module
    ${modules_output_path}/client_qml.msm
    SOURCE_FILES
        ${CMAKE_CURRENT_BINARY_DIR}/client_qml.wixobj
        ${CMAKE_CURRENT_BINARY_DIR}/client_qml_module.wixobj)
