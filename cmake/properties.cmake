set(client.binary.name "client-bin")
set(applauncher.binary.name "applauncher")
set(minilauncher.binary.name "applauncher-bin")

if(WIN32)
    set(client.binary.name "${product.name}.exe")
    set(applauncher.binary.name "applauncher.exe")
    set(minilauncher.binary.name "${product.name} Launcher.exe")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(client.binary.name "${display.product.name}")
endif()

set(client.mediafolder.name "${product.name} Media")
