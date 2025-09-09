## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

set(APPNAME nx_vms_web_assembly_client)
set(APPEXPORTNAME nx_vms_web_assembly_client_entry)

nx_add_target(nx_vms_web_assembly_client EXECUTABLE PRIVATE_LIBS embind Qt6::Quick nx_utils)
qt6_import_qml_plugins(nx_vms_web_assembly_client)

include("${QT_DIR}/lib/cmake/Qt6/QtWasmHelpers.cmake")
qt_internal_setup_wasm_target_properties(nx_vms_web_assembly_client)

target_link_options(nx_vms_web_assembly_client PRIVATE "SHELL:-s INITIAL_MEMORY=50MB")
target_link_options(nx_vms_web_assembly_client PRIVATE "SHELL:-s MAXIMUM_MEMORY=4GB")
target_link_options(nx_vms_web_assembly_client PRIVATE "SHELL:-s PTHREAD_POOL_SIZE=4")
target_link_options(nx_vms_web_assembly_client PRIVATE "SHELL:-s MODULARIZE=1")
target_link_options(nx_vms_web_assembly_client PRIVATE "SHELL:-s EXPORT_NAME=${APPEXPORTNAME}")
target_link_options(nx_vms_web_assembly_client PRIVATE
    "SHELL:-s EXPORTED_RUNTIME_METHODS=UTF16ToString,stringToUTF16,JSEvents,specialHTMLTargets,FS,callMain")
target_link_options(nx_vms_web_assembly_client PRIVATE
    "SHELL:-s EXPORTED_FUNCTIONS=_main,__embind_initialize_bindings")

configure_file("${QT_DIR}/plugins/platforms/wasm_shell.html"
    "${CMAKE_CURRENT_BINARY_DIR}/nx_vms_web_assembly_client.html" @ONLY)
nx_store_known_file("${CMAKE_CURRENT_BINARY_DIR}/nx_vms_web_assembly_client.html")
add_custom_command(
    TARGET nx_vms_web_assembly_client POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_BINARY_DIR}/nx_vms_web_assembly_client.html"
        "${CMAKE_BINARY_DIR}/bin/nx_vms_web_assembly_client.html")
nx_store_known_file("${CMAKE_BINARY_DIR}/bin/nx_vms_web_assembly_client.html")
add_custom_command(
    TARGET nx_vms_web_assembly_client POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${QT_DIR}/plugins/platforms/qtloader.js"
        "${CMAKE_BINARY_DIR}/bin/qtloader.js")
nx_store_known_file("${CMAKE_BINARY_DIR}/bin/qtloader.js")
add_custom_command(
    TARGET nx_vms_web_assembly_client POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${QT_DIR}/plugins/platforms/qtlogo.svg"
        "${CMAKE_BINARY_DIR}/bin/qtlogo.svg")
nx_store_known_file("${CMAKE_BINARY_DIR}/bin/qtlogo.svg")
