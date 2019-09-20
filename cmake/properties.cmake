# This file is loaded after the customization. It contains logic to set complex variables, combined
# from other customizable variables. Also it contains defaults for the rarely overridden variables.
# Finally, we can declare here some variables, that are not overridden, but semantically related to
# the customizable product options.

# Transform the variables for the migration simplicity.
# TODO: #GDM Replace old variables with new ones.
set(build_nxtool OFF)
if(customization.nxtool.enabled)
    set(build_nxtool ON)
endif()

set(build_paxton OFF)
if(customization.paxton.enabled)
    set(build_paxton ON)
endif()

set(build_mobile OFF)
if(customization.mobile.enabled)
    set(build_mobile ON)
endif()

set(enable_hanwha false) # TODO: rename
if(customization.desktop.supportsHanwha)
    set(enable_hanwha true)
endif()

set(vmax false) # TODO: rename
if(customization.desktop.supportsVmax)
    set(vmax true)
endif()

# Overridden in hanwha, qulu
set(shortCloudName "Cloud")
if(customization.advanced.useFullCloudNameEverywhere)
    set(shortCloudName "${customization.cloudName}")
endif()

# Overridden in ionetworks, ionetworks_cn
set(windowsInstallPath "${customization.companyName}")
if(customization.advanced.customWindowsInstallPath)
    set(windowsInstallPath "${customization.advanced.customWindowsInstallPath}")
endif()


# Internal names must not be changed in all rebrandings
set(mobileClientInternalName "${customization.companyName} ${customization.vmsId} Mobile Client")
set(desktopClientInternalName "${customization.companyName} ${customization.vmsId} Client")
set(mediaFolderName "${customization.vmsId} Media")

# Component display names
set(client.display.name "${customization.vmsName} Client")
if(customization.advanced.customClientDisplayName)
    # Overridden in metavms
    set(client.display.name "${customization.advanced.customClientDisplayName}")
endif()

set(mediaserver_service_name "${customization.companyName} Server")
if(customization.advanced.customMediaserverServiceName)
    # Overridden in metavms
    set(mediaserver_service_name "${customization.advanced.customMediaserverServiceName}")
endif()

set(mediaserver_application_name "${customization.companyName} Media Server")
if(customization.advanced.customMediaserverApplicationName)
    # Overridden in metavms
    set(mediaserver_application_name "${customization.advanced.customMediaserverApplicationName}")
endif()

set(applauncher.name "${customization.companyName} ${customization.vmsName} Launcher")
set(traytool.name "${customization.vmsName} Tray Assistant")
set(mediaserver.display.name "${customization.vmsName} Media Server")
set(testcamera.name "${customization.companyName} ${customization.vmsName} Test Camera")

# Binary names and pathes
if(WINDOWS)
    set(client.binary.name "${customization.vmsId}.exe")
    set(applauncher.binary.name "applauncher.exe")
    set(minilauncher.binary.name "${customization.vmsId} Launcher.exe")
    set(testcamera.binary.name "testcamera.exe")
    set(installation.root "C:/Program Files/${windowsInstallPath}/${customization.vmsName}")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(client.binary.name "${customization.vmsName}")
    set(applauncher.binary.name "applauncher-bin")
    set(minilauncher.binary.name "applauncher")
    set(testcamera.binary.name "testcamera")
    set(installation.root "/Applications/")
else()
    set(client.binary.name "client-bin")
    set(applauncher.binary.name "applauncher-bin")
    set(minilauncher.binary.name "applauncher")
    set(testcamera.binary.name "testcamera")
    set(installation.root "/opt/${customization.companyId}")
endif()

# Localization
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

# Temporary workaround cms issue
if(customization.customLanguages AND NOT customization.customLanguages STREQUAL "[]")
    set(translations ${customization.customLanguages})
endif()

# Other variables
set(launcher.version.file "launcher.version")
string(TIMESTAMP current_year %Y UTC)
set(nx_copyright "Copyright (c) 2011-${current_year} Network Optix")

if(customization.advanced.disableCodeSigning)
    set(codeSigning OFF)
endif()

if(targetDevice MATCHES "bpi")
    set(liteMode "true")
else()
    set(liteMode "false")
endif()

set(apple_team_id "L6FE34GJWM")

if(eulaVersionOverride)
    set(customization.eulaVersion ${eulaVersionOverride})
endif()

#TODO: #GDM This can actually be overwritten (vista, ipera)
set(product.updateGeneratorUrl "http://updates.hdw.mx/upcombiner/upcombine")
set(product.updateFeedUrl "http://updates.vmsproxy.com/updates.json")
set(product.freeLicenseCount 4)
set(product.freeLicenseIsTrial true)

# This property was overridden in the vista customization only. Not actual as we dropped lite mode.
set(product.liteDeviceName "nx1")

# Compatibility layer
set(installerLanguage "${customization.defaultLanguage}")
