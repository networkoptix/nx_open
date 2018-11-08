set(installerLanguage "en_US")
set(installerMsiFile "${distribution_output_dir}/${paxton_plugin_distribution_name}.msi")
set(themeFile "${repositoryRootDir}/vms/distribution/wix/common/theme.xml")
set(localizationFile "${repositoryRootDir}/vms/distribution/wix/common/theme_${installerLanguage}.wxl")
set(paxtonPluginName "${display.product.name} Paxton Plugin")

if(codeSigning)
    set(installerUnsignedExeFile "${CMAKE_CURRENT_BINARY_DIR}/${paxton_plugin_distribution_name}-unsigned.exe")
else()
    set(installerUnsignedExeFile "${distribution_output_dir}/${paxton_plugin_distribution_name}-unsigned.exe")
endif()

nx_configure_file(app_info.wxi.in ${CMAKE_CURRENT_BINARY_DIR}/app_info.wxi)

add_custom_command(
    OUTPUT package.wixobj
    COMMAND ${wix_directory}/bin/candle.exe
        ${CMAKE_CURRENT_SOURCE_DIR}/package.wxs
        -I${CMAKE_CURRENT_BINARY_DIR}
        -nologo
        -out package.wixobj
    MAIN_DEPENDENCY
        ${CMAKE_CURRENT_SOURCE_DIR}/package.wxs
    DEPENDS
        paxton_plugin
        ${CMAKE_CURRENT_BINARY_DIR}/app_info.wxi
)

add_custom_command(
    OUTPUT ${installerMsiFile}
    COMMAND ${wix_directory}/bin/light.exe
        ${CMAKE_CURRENT_BINARY_DIR}/package.wixobj
        -nologo
        -out ${installerMsiFile}
        -pdbout ${CMAKE_CURRENT_BINARY_DIR}/package.wixpdb
    MAIN_DEPENDENCY
        package.wixobj
)

add_custom_command(
    OUTPUT bundle.wixobj
    COMMAND ${wix_directory}/bin/candle.exe
        ${CMAKE_CURRENT_SOURCE_DIR}/bundle.wxs
        -I${CMAKE_CURRENT_BINARY_DIR}
        -nologo
        -ext WixBalExtension
        -out bundle.wixobj
    MAIN_DEPENDENCY
        ${CMAKE_CURRENT_SOURCE_DIR}/bundle.wxs
    DEPENDS
        ${localizationFile}
        ${CMAKE_CURRENT_BINARY_DIR}/app_info.wxi
)

add_custom_command(
    OUTPUT ${installerUnsignedExeFile}
    COMMAND ${wix_directory}/bin/light.exe
        ${CMAKE_CURRENT_BINARY_DIR}/bundle.wixobj
        -out ${installerUnsignedExeFile}
        -nologo
        -ext WixBalExtension
        -pdbout ${CMAKE_CURRENT_BINARY_DIR}/bundle.wixpdb
    MAIN_DEPENDENCY
        bundle.wixobj
    DEPENDS
        ${themeFile}
        ${installerMsiFile}
)

add_custom_target(paxton_plugin_installer ALL
    COMMENT "Creating paxton plugin installer..."
    DEPENDS
        ${installerMsiFile}
        ${installerUnsignedExeFile}
)

if(codeSigning)
    set(engineExeFile "${CMAKE_CURRENT_BINARY_DIR}/engine.exe")
    set(installerExeFile "${distribution_output_dir}/${paxton_plugin_distribution_name}.exe")

    set(signCommand "")
    nx_get_windows_sign_command(signCommand)

    add_custom_command(
        OUTPUT ${installerExeFile}
        COMMAND ${wix_directory}/bin/insignia.exe
            -ib ${installerUnsignedExeFile}
            -o ${engineExeFile}

        COMMAND ${signCommand} ${engineExeFile}

        COMMAND ${wix_directory}/bin/insignia.exe
            -ab ${engineExeFile} ${installerUnsignedExeFile}
            -o ${installerExeFile}

        COMMAND ${signCommand} ${installerExeFile}
        MAIN_DEPENDENCY
            ${installerUnsignedExeFile}
    )

    add_custom_target(paxton_plugin_signed_installer ALL
        COMMENT "Signing paxton plugin installer..."
        DEPENDS
            ${installerExeFile}
    )
endif()
