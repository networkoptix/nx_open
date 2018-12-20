set(qtkeychain_dir ${CMAKE_SOURCE_DIR}/artifacts/qtkeychain)

include(GenerateExportHeader)

if(WINDOWS)
    add_definitions(-DUSE_CREDENTIAL_STORE=1)
endif()

set(sources
    ${qtkeychain_dir}/keychain.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/qkeychain_export.h
    ${qtkeychain_dir}/keychain.h
)

set(libs Qt5::Core)

if(WIN32)
    list(APPEND sources ${qtkeychain_dir}/keychain_win.cpp)
endif()

if(APPLE)
    if(IOS)
        list(APPEND sources ${qtkeychain_dir}/keychain_ios.mm)
    else()
        list(APPEND sources ${qtkeychain_dir}/keychain_mac.cpp)
    endif()

    find_library(COREFOUNDATION_LIBRARY CoreFoundation REQUIRED)
    list(APPEND libs ${COREFOUNDATION_LIBRARY})

    find_library(SECURITY_LIBRARY Security REQUIRED)
    list(APPEND libs ${SECURITY_LIBRARY})
endif()

if(UNIX AND NOT APPLE AND NOT ANDROID)
    list(APPEND sources
        ${qtkeychain_dir}/keychain_unix.cpp
        ${qtkeychain_dir}/gnomekeyring.cpp
        ${qtkeychain_dir}/libsecret.cpp
        ${qtkeychain_dir}/plaintextstore.cpp
    )

    find_package(Qt5DBus REQUIRED)
    list(APPEND libs Qt5::DBus)
    qt5_add_dbus_interface(sources
        ${qtkeychain_dir}/org.kde.KWallet.xml kwallet_interface KWalletInterface)

    find_package(PkgConfig REQUIRED)

    pkg_check_modules(LIBSECRET libsecret-1 REQUIRED)
    add_definitions(-DHAVE_LIBSECRET=1)
    include_directories(${LIBSECRET_INCLUDE_DIRS})
    list(APPEND libs ${LIBSECRET_LIBRARIES})
endif()

qt5_wrap_cpp(moc_sources
    ${qtkeychain_dir}/keychain.h
    ${qtkeychain_dir}/keychain_p.h
    ${qtkeychain_dir}/gnomekeyring_p.h
    OPTIONS -I${QT_DIR}/include
)

add_library(qtkeychain SHARED ${sources} ${moc_sources})

target_link_libraries(qtkeychain PUBLIC ${libs})
target_include_directories(qtkeychain
    PUBLIC
        ${qtkeychain_dir}
        ${CMAKE_CURRENT_BINARY_DIR}
)

generate_export_header(qtkeychain
    EXPORT_FILE_NAME qkeychain_export.h
    EXPORT_MACRO_NAME QKEYCHAIN_EXPORT
)
nx_store_known_file(${CMAKE_CURRENT_BINARY_DIR}/qkeychain_export.h)

set_target_properties(qtkeychain PROPERTIES
    VERSION 0.9.0
    MACOSX_RPATH 1
    INSTALL_RPATH_USE_LINK_PATH TRUE
)
