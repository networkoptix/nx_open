file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/resources")

nx_configure_file(
    maven/filter-resources/app_info_impl.cpp
    ${CMAKE_CURRENT_BINARY_DIR})

if(MACOSX)
    set(mac_sources src/utils/mac_utils.mm)
endif()

nx_add_target(common LIBRARY
    ADDITIONAL_SOURCES
        "${CMAKE_CURRENT_BINARY_DIR}/app_info_impl.cpp"
        ${mac_sources}
    ADDITIONAL_RESOURCES
        "${CMAKE_CURRENT_BINARY_DIR}/resources"
    PUBLIC_LIBS
        Qt5::Gui Qt5::Concurrent Qt5::Multimedia
        Qt5::Xml Qt5::XmlPatterns Qt5::Sql
        ffmpeg
        detection_plugin_interface
        nx_network nx_utils nx_fusion
)

if(NOT ANDROID AND NOT IOS)
    target_link_libraries(common PUBLIC quazip)
endif()

target_include_directories(common
    PRIVATE
        ${Qt5Core_PRIVATE_INCLUDE_DIRS}
)

if(WINDOWS)
    target_link_libraries(common PUBLIC Winmm)
endif()

if(MACOSX)
    find_library(CoreGraphics_LIBRARY CoreGraphics)
    mark_as_advanced(CoreGraphics_LIBRARY)
    target_link_libraries(common PUBLIC ${CoreGraphics_LIBRARY})
endif()

if(targetDevice STREQUAL "isd")
    target_compile_definitions(common PRIVATE ISD_JAGUAR)
endif()
