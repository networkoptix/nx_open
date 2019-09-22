nx_add_test(nx_utils_ut
    WERROR
    PUBLIC_LIBS
        nx_utils
        nx_fusion
        Qt5::Network)

set(mocableFiles
    ${CMAKE_CURRENT_SOURCE_DIR}/src/qmetaobject_helper_ut.h)

nx_add_qt_mocables(nx_utils_ut ${mocableFiles}
    INCLUDE_DIRS
    "${CMAKE_CURRENT_SOURCE_DIR}/src")
