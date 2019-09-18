set(liteDeviceName "microserver")
set(shortCloudName "Cloud")

set(defaultWebPages "{}")

set(eulaVersion 1)

set(useMetaVersion OFF)

# Localization
set(installerLanguage "en_US")
set(installer.cultures "en-us")
set(translations
    en_US
    en_GB
    fr_FR
    cs_CZ
    de_DE
    fi_FI
    ru_RU
    es_ES
    it_IT
    ja_JP
    ko_KR
    tr_TR
    zh_CN
    zh_TW
    he_IL
    hu_HU
    nl_BE
    nl_NL
    no_NO
    pl_PL
    pt_BR
    pt_PT
    uk_UA
    vi_VN
    th_TH
    fi_FI
    sv_SE
)

set(defaultTranslation en_US)

# Defaults to ${customization.companyName} in the properties.cmake
set(windowsInstallPath "")



#TODO: #GDM #FIXME How to write it better?
if(customization.translations)
    set(product.translations ${customization.translations})
else()
    set(product.translations ${product.defaultTranslations})
endif()
