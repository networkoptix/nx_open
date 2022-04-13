## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

include("${CMAKE_CURRENT_LIST_DIR}/qt_utils/moc_utils.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/qt_utils/translation_utils.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/qt_utils/qrc_utils.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/qt_utils/fix_qt_vars.cmake")

list(APPEND CMAKE_PREFIX_PATH ${QT_DIR})
list(APPEND CMAKE_FIND_ROOT_PATH ${QT_DIR})
if(MACOSX)
    set(CMAKE_FRAMEWORK_PATH "${QT_DIR}/lib")
    list(APPEND CMAKE_INSTALL_RPATH ${QT_DIR}/lib)
endif()

find_package(OpenSSL REQUIRED)

find_package(Qt5
    COMPONENTS
        LinguistTools
        Core
        Gui
        Network
        Xml
        Sql
        Concurrent
        Multimedia
        Qml
)
