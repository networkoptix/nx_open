set(client.name "${company.name} ${product.name} Client")
set(client.binary.name "client-bin")
set(client.display.name "${display.product.name} Client")
set(mobile_client.name "${company.name} ${product.name} Mobile Client")
set(mobile_client.display.name "${display.product.name} Mobile Client")
set(display.mobile.name "${display.product.name} Mobile")
set(applauncher.binary.name "applauncher")
set(applauncher.name "${company.name} ${display.product.name} Launcher")
set(traytool.name "${display.product.name} Tray Assistant")
set(minilauncher.binary.name "applauncher-bin")
set(mediaserver.name "${company.name} Media Server")
set(mediaserver.display.name "${display.product.name} Media Server")

if(WINDOWS)
    set(client.binary.name "${product.name}.exe")
    set(applauncher.binary.name "applauncher.exe")
    set(minilauncher.binary.name "${product.name} Launcher.exe")
    set(installation.root "C:/Program Files/${company.name}/${display.product.name}")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(client.binary.name "${display.product.name}")
    set(installation.root "/Applications/")
    set(protocol_handler_app_name "${display.product.name} Client.app")
endif()

set(client.mediafolder.name "${product.name} Media")

if(targetDevice MATCHES "bpi|bananapi|rpi|edge1")
    set(liteMode "true")
else()
    set(liteMode "false")
endif()

set(launcher.version.file "launcher.version")
set(installation.root "/opt/${deb.customization.company.name}")

hg_changeset(${PROJECT_SOURCE_DIR} changeSet)
hg_branch(${PROJECT_SOURCE_DIR} branch)
