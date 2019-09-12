# Overridden in metavms
nx_set_variable_if_empty(mediaserver_service_name "${company.name} Server")
nx_set_variable_if_empty(mediaserver_application_name "${company.name} Media Server")

# Overridden in ionetworks, ionetworks_cn
nx_set_variable_if_empty(windowsInstallPath "${company.name}")

# TODO: GDM Rename to 'Client Settings Key and use explicitly'
set(client.name "${company.name} ${product.name} Client")

set(client.display.name "${customization.vmsName} Client")
set(applauncher.name "${company.name} ${customization.vmsName} Launcher")
set(traytool.name "${customization.vmsName} Tray Assistant")
set(mediaserver.display.name "${customization.vmsName} Media Server")
set(client.mediafolder.name "${product.name} Media")
set(testcamera.name "${company.name} ${customization.vmsName} Test Camera")
set(client.binary.name "client-bin")
set(applauncher.binary.name "applauncher-bin")
set(minilauncher.binary.name "applauncher")
set(testcamera.binary.name "testcamera")
set(installation.root "/opt/${deb.customization.company.name}")
set(launcher.version.file "launcher.version")
string(TIMESTAMP current_year %Y UTC)
set(nx_copyright "Copyright (c) 2011-${current_year} Network Optix")

if(WINDOWS)
    set(client.binary.name "${product.name}.exe")
    set(applauncher.binary.name "applauncher.exe")
    set(minilauncher.binary.name "${product.name} Launcher.exe")
    set(testcamera.binary.name "testcamera.exe")
    set(installation.root "C:/Program Files/${windowsInstallPath}/${customization.vmsName}")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(client.binary.name "${customization.vmsName}")
    set(installation.root "/Applications/")
endif()

if(targetDevice MATCHES "bpi")
    set(liteMode "true")
else()
    set(liteMode "false")
endif()

set(apple_team_id "L6FE34GJWM")

if(eulaVersionOverride)
    set(eulaVersion ${eulaVersionOverride})
endif()

set(enable_hanwha false)
if(customization.desktop.supportsHanwha)
    set(enable_hanwha true)
endif()
