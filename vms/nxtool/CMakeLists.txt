nx_configure_file(nxtool_app_info_impl.cpp ${CMAKE_CURRENT_BINARY_DIR})

file(GLOB_RECURSE QML_FILES static-resources/src/qml/*.qml)

nx_add_target(nxtool EXECUTABLE SIGNED
    RC_FILE
        "nxtool.rc"
    ADDITIONAL_SOURCES
        "${CMAKE_CURRENT_BINARY_DIR}/nxtool_app_info_impl.cpp"
    OTHER_SOURCES
        ${QML_FILES}
    PUBLIC_LIBS
        Qt5::Core
        Qt5::Gui
        Qt5::Network
        Qt5::Quick
        Qt5::Qml)

if(WINDOWS)
    target_link_libraries(nxtool PRIVATE ws2_32)
endif()
