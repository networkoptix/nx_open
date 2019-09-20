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

# Overridden in metavms
nx_set_variable_if_empty(mediaserver_service_name "${customization.companyName} Server")
nx_set_variable_if_empty(mediaserver_application_name "${customization.companyName} Media Server")

# Overridden in ionetworks, ionetworks_cn
nx_set_variable_if_empty(windowsInstallPath "${customization.companyName}")

# Internal names must not be changed in all rebrandings
set(mobileClientInternalName "${customization.companyName} ${customization.desktop.internalName} Mobile Client")
set(desktopClientInternalName "${customization.companyName} ${customization.desktop.internalName} Client")
set(mediaFolderName "${customization.desktop.internalName} Media")

# Component display names
set(client.display.name "${customization.vmsName} Client")
set(applauncher.name "${customization.companyName} ${customization.vmsName} Launcher")
set(traytool.name "${customization.vmsName} Tray Assistant")
set(mediaserver.display.name "${customization.vmsName} Media Server")
set(testcamera.name "${customization.companyName} ${customization.vmsName} Test Camera")

# Binary names and pathes
if(WINDOWS)
    set(client.binary.name "${customization.desktop.internalName}.exe")
    set(applauncher.binary.name "applauncher.exe")
    set(minilauncher.binary.name "${customization.desktop.internalName} Launcher.exe")
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
    set(installation.root "/opt/${deb.customization.company.name}")
endif()

# Other variables
set(launcher.version.file "launcher.version")
string(TIMESTAMP current_year %Y UTC)
set(nx_copyright "Copyright (c) 2011-${current_year} Network Optix")

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
