nx_set_variable_if_empty(client.name "${company.name} ${product.name} Client")
nx_set_variable_if_empty(client.display.name "${display.product.name} Client")
nx_set_variable_if_empty(mobile_client.name "${company.name} ${product.name} Mobile Client")
nx_set_variable_if_empty(mobile_client.display.name "${display.product.name} Mobile Client")
nx_set_variable_if_empty(applauncher.name "${company.name} ${display.product.name} Launcher")
nx_set_variable_if_empty(traytool.name "${display.product.name} Tray Assistant")
nx_set_variable_if_empty(mediaserver.name "${company.name} Media Server")
nx_set_variable_if_empty(mediaserver.display.name "${display.product.name} Media Server")

set(client.binary.name "client-bin")
set(applauncher.binary.name "applauncher")
set(minilauncher.binary.name "applauncher-bin")

if(WINDOWS)
    set(client.binary.name "${product.name}.exe")
    set(applauncher.binary.name "applauncher.exe")
    set(minilauncher.binary.name "${product.name} Launcher.exe")
    set(installation.root "C:/Program Files/${company.name}/${display.product.name}")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(client.binary.name "${display.product.name}")
    set(installation.root "/Applications/")
    nx_set_variable_if_empty(protocol_handler_app_name "${display.product.name} Client.app")
endif()

nx_set_variable_if_empty(client.mediafolder.name "${product.name} Media")

set(liteMode "false")
set(launcher.version.file "launcher.version")
set(installation.root "/opt/${deb.customization.company.name}")

set(apple_team_id "L6FE34GJWM")

hg_changeset(${PROJECT_SOURCE_DIR} changeSet)
hg_branch(${PROJECT_SOURCE_DIR} branch)
