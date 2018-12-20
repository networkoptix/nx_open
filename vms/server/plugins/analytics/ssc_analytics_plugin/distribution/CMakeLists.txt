set(installerLanguage "en_US")
set(installerMsiFile "${CMAKE_CURRENT_BINARY_DIR}/${ssc_analytics_plugin_distribution_name}.msi")
set(themeFile "${CMAKE_SOURCE_DIR}/vms/distribution/wix/common/theme.xml")
set(localizationFile "${CMAKE_SOURCE_DIR}/vms/distribution/wix/common/theme_${installerLanguage}.wxl")

# Project is actual to ionetworks customization only, so hardcode.
if(customization STREQUAL "ionetworks")
    set(productId "{557E96C5-8DFD-4D2B-92EC-584D4CD404AE}")
    set(productUpgradeCode "{6604116D-33EB-4204-866B-1AFC4AC1714D}")
    set(bundleUpgradeCode "{48AD07BB-7A58-45D1-A39D-0857C07B450A}")
else()
    message(FATAL_ERROR "Only ionetworks customization is supported.")
endif()

if(codeSigning)
    set(unsignedDistributionDir "${CMAKE_CURRENT_BINARY_DIR}")
else()
    set(unsignedDistributionDir "${distribution_output_dir}")
endif()

set(installerUnsignedExeFile "${unsignedDistributionDir}/${ssc_analytics_plugin_distribution_name}-unsigned.exe")

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
        ssc_analytics_plugin
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

add_custom_target(ssc_installer ALL
    COMMENT "Creating ssc analytics plugin distribution..."
    DEPENDS
        ${installerMsiFile}h
        ${installerUnsignedExeFile}
)

if(codeSigning)
    set(engineExeFile "${CMAKE_CURRENT_BINARY_DIR}/engine.exe")
    set(installerExeFile "${distribution_output_dir}/${ssc_analytics_plugin_distribution_name}.exe")

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

    add_custom_target(ssc_signed_installer ALL
        COMMENT "Signing ssc analytics plugin installer..."
        DEPENDS
            ${installerExeFile}
    )
endif()
