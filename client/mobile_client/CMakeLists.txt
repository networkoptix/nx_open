find_package(Qt5 COMPONENTS WebChannel WebSockets)

set(product.title "${mobile_client.name}")
set(product.display.title "${mobile_client.display.name}")

nx_configure_file(
    maven/filter-resources/mobile_client_app_info_impl.cpp
    ${CMAKE_CURRENT_BINARY_DIR})

file(GLOB_RECURSE QML_FILES static-resources/qml/*.qml static-resources/qml/*.js)

set(target_args
    ADDITIONAL_SOURCES
        "${CMAKE_CURRENT_BINARY_DIR}/mobile_client_app_info_impl.cpp"
    ADDITIONAL_RESOURCES
        "${customization_dir}/mobile_client/resources"
    OTHER_SOURCES
        ${QML_FILES}
    PUBLIC_LIBS
        Qt5::WebChannel Qt5::WebSockets
        qtsingleguiapplication
        nx_kit
        nx_client_core)

if(ANDROID)
    nx_add_target(mobile_client LIBRARY LIBRARY_TYPE SHARED ${target_args})
else()
    nx_add_target(mobile_client EXECUTABLE ${target_args})
endif()

if(ANDROID)
    target_link_libraries(mobile_client PUBLIC GLESv2 z log)
endif()
