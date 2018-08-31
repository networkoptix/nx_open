set(installerLanguage "en_US")

nx_configure_file(app_info.wxi.in ${CMAKE_CURRENT_BINARY_DIR}/app_info.wxi)
nx_configure_file(theme.xml.in ${CMAKE_CURRENT_BINARY_DIR}/theme.xml)

add_custom_command(
    OUTPUT package.wixobj
    COMMAND ${wix_directory}/bin/candle.exe
        "${CMAKE_CURRENT_SOURCE_DIR}/package.wxs"
        "-I${CMAKE_CURRENT_BINARY_DIR}"
        "-nologo"
        "-out" "package.wixobj"
    MAIN_DEPENDENCY
        ${CMAKE_CURRENT_SOURCE_DIR}/package.wxs
    DEPENDS
        ${CMAKE_CURRENT_BINARY_DIR}/app_info.wxi
)

add_custom_command(
    OUTPUT ${paxtonDriverOutputName}.msi
    COMMAND ${wix_directory}/bin/light.exe
        "${CMAKE_CURRENT_BINARY_DIR}/package.wixobj"
        "-nologo"
        "-out" "${CMAKE_CURRENT_BINARY_DIR}/${paxtonDriverOutputName}.msi"
    MAIN_DEPENDENCY
        package.wixobj
)

add_custom_command(
    OUTPUT bundle.wixobj
    COMMAND ${wix_directory}/bin/candle.exe
        "${CMAKE_CURRENT_SOURCE_DIR}/bundle.wxs"
        "-I${CMAKE_CURRENT_BINARY_DIR}"
        "-nologo"
        "-ext" "WixBalExtension"
        "-out" "bundle.wixobj"
    MAIN_DEPENDENCY
        ${CMAKE_CURRENT_SOURCE_DIR}/bundle.wxs
    DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/theme_en_US.wxl
        ${CMAKE_CURRENT_BINARY_DIR}/app_info.wxi
)

add_custom_command(
    OUTPUT ${paxtonDriverOutputName}.exe
    COMMAND ${wix_directory}/bin/light.exe
        "${CMAKE_CURRENT_BINARY_DIR}/bundle.wixobj"
        "-out" "${CMAKE_CURRENT_BINARY_DIR}/${paxtonDriverOutputName}.exe"
        "-nologo"
        "-ext" "WixBalExtension"
    MAIN_DEPENDENCY
        bundle.wixobj
    DEPENDS
        ${CMAKE_CURRENT_BINARY_DIR}/theme.xml
        ${paxtonDriverOutputName}.msi
)

add_custom_target(paxton_plugin_installer ALL
    COMMENT "Creating paxton plugin installer..."
    DEPENDS
        paxton_plugin
        ${paxtonDriverOutputName}.msi
        ${paxtonDriverOutputName}.exe
)


