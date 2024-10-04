## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

find_package(concurrentqueue REQUIRED)

if(IOS)
    set(ios_sources "src/nx/utils/log/log_ios.mm")
endif()

set(aes_library_root "${open_source_root}/artifacts/tiny_aes_c")
file(GLOB_RECURSE aes_library_sources "${aes_library_root}/src/tiny_aes_c/*")

nx_add_target(nx_utils LIBRARY
    WERROR_IF NOT WINDOWS
    NO_MOC
    ADDITIONAL_SOURCES
        ${ios_sources}
        ${aes_library_sources}
    PUBLIC_LIBS
        Boost::headers
        OpenSSL::OpenSSL
        Qt6::Core Qt6::Xml
        quazip
        nx_branding
        nx_build_info
        nx_kit
        nx_reflect
    PRIVATE_LIBS
        concurrentqueue::concurrentqueue
    FOLDER common/libs
)

set(mocableFiles
    ${CMAKE_CURRENT_SOURCE_DIR}/src/nx/utils/counter.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/nx/utils/virtual_property.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/nx/utils/safe_direct_connection.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/nx/utils/software_version.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/nx/utils/uuid.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/nx/utils/url.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/nx/utils/crypt/crypted_file_stream.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/nx/utils/thread/long_runnable.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/nx/utils/thread/thread.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/nx/utils/async_handler_executor_detail.h)

nx_add_qt_mocables(nx_utils ${mocableFiles}
    INCLUDE_DIRS
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_compile_definitions(nx_utils
    PRIVATE NX_UTILS_API=${API_EXPORT_MACRO}
    INTERFACE NX_UTILS_API=${API_IMPORT_MACRO})

target_include_directories(nx_utils
    PRIVATE
        ${Qt6Core_PRIVATE_INCLUDE_DIRS}
        ${aes_library_root}/src
)

if (IOS)
    target_compile_definitions(nx_utils
        PRIVATE HAVE_UNISTD_H)
endif()

if(WINDOWS)
    target_link_libraries(nx_utils PRIVATE DbgHelp ws2_32)
endif()

if(LINUX)
    target_link_libraries(nx_utils PRIVATE dl)
endif()

if(LINUX AND NOT ANDROID)
    target_link_libraries(nx_utils PRIVATE pthread)
endif()

if(LINUX)
    target_link_libraries(nx_utils PRIVATE dl)
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
