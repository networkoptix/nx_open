nx_configure_file(
    app_info_impl.cpp
    ${CMAKE_CURRENT_BINARY_DIR})

set(library_type)
if(WINDOWS)
    set(library_type LIBRARY_TYPE SHARED)
endif()

if(IOS)
    set(ios_sources "src/nx/utils/log/log_ios.mm")
endif()

nx_add_target(nx_utils LIBRARY ${library_type} WERROR NO_MOC
    ADDITIONAL_SOURCES
        "${CMAKE_CURRENT_BINARY_DIR}/app_info_impl.cpp"
        ${ios_sources}
    OTHER_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/qt5.natvis"
    PUBLIC_LIBS
        boost openssl Qt5::Core Qt5::Xml
        nx_kit
)

set(mocableFiles
    ${CMAKE_CURRENT_SOURCE_DIR}/src/nx/utils/counter.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/nx/utils/safe_direct_connection.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/nx/utils/uuid.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/nx/utils/url.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/nx/utils/thread/long_runnable.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/nx/utils/thread/thread.h)

nx_add_qt_mocables(nx_utils ${mocableFiles}
    INCLUDE_DIRS
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_compile_definitions(nx_utils
    PRIVATE NX_UTILS_API=${API_EXPORT_MACRO}
    INTERFACE NX_UTILS_API=${API_IMPORT_MACRO})

target_include_directories(nx_utils
    PRIVATE
        ${Qt5Core_PRIVATE_INCLUDE_DIRS}
)

if(WINDOWS)
    target_link_libraries(nx_utils PRIVATE DbgHelp ws2_32)
endif()

if(LINUX AND NOT ANDROID)
    target_link_libraries(nx_utils PRIVATE pthread)
endif()

if(MACOSX)
    find_library(Foundation_LIBRARY Foundation)
    mark_as_advanced(Foundation_LIBRARY)
    target_link_libraries(nx_utils PUBLIC ${Foundation_LIBRARY})
endif()

if(NOT WINDOWS)
    target_link_libraries(nx_utils PRIVATE z)
endif()

if(withTests)
    add_subdirectory(unit_tests)
endif()
