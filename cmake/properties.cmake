nx_set_variable_if_empty(client.name "${company.name} ${product.name} Client")
nx_set_variable_if_empty(client.display.name "${display.product.name} Client")
nx_set_variable_if_empty(mobile_client.name "${company.name} ${product.name} Mobile Client")
nx_set_variable_if_empty(mobile_client.display.name "${display.product.name} Mobile Client")
nx_set_variable_if_empty(applauncher.name "${company.name} ${display.product.name} Launcher")
nx_set_variable_if_empty(traytool.name "${display.product.name} Tray Assistant")
nx_set_variable_if_empty(mediaserver.display.name "${display.product.name} Media Server")
nx_set_variable_if_empty(mediaserver_service_name "${company.name} Server")
nx_set_variable_if_empty(mediaserver_application_name "${company.name} Media Server")
nx_set_variable_if_empty(client.mediafolder.name "${product.name} Media")
nx_set_variable_if_empty(nxtool.name "${company.name} ${display.product.name} Server Tool")
nx_set_variable_if_empty(testcamera.name "${company.name} ${display.product.name} Test Camera")
nx_set_variable_if_empty(windowsInstallPath "${company.name}")

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
    set(installation.root "C:/Program Files/${windowsInstallPath}/${display.product.name}")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(client.binary.name "${display.product.name}")
    set(installation.root "/Applications/")
endif()

if(targetDevice MATCHES "bpi|bananapi|rpi|edge1")
    set(liteMode "true")
else()
    set(liteMode "false")
endif()

set(apple_team_id "L6FE34GJWM")
