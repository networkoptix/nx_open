nx_configure_file(app_info.wxi.in ${CMAKE_CURRENT_BINARY_DIR}/app_info.wxi)

add_custom_command(
    OUTPUT package.wixobj
    COMMAND ${wix_directory}/bin/candle.exe
        "${CMAKE_CURRENT_SOURCE_DIR}/package.wxs"
        "-I${CMAKE_CURRENT_BINARY_DIR}"
        "-nologo"
    MAIN_DEPENDENCY ${CMAKE_CURRENT_SOURCE_DIR}/package.wxs
    DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/app_info.wxi
)

add_custom_command(
    OUTPUT ${paxtonDriverOutputName}.msi
    COMMAND ${wix_directory}/bin/light.exe
        "${CMAKE_CURRENT_BINARY_DIR}/package.wixobj"
        "-out"
        "${CMAKE_CURRENT_BINARY_DIR}/${paxtonDriverOutputName}.msi"
        "-nologo"
    MAIN_DEPENDENCY package.wixobj
)

add_custom_target(paxton_plugin_installer ALL
    COMMENT "Creating paxton plugin installer..."
    DEPENDS
        paxton_plugin
        ${paxtonDriverOutputName}.msi
)


