## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

project(WixCustomActions)

if (NOT CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    message(FATAL_ERROR "Unsupported compiler ${CMAKE_CXX_COMPILER_ID}")
endif()

set(vsPath "vs2015")
if (CMAKE_CXX_COMPILER_VERSION MATCHES "19.*")
    set(vsPath "vs2017")
endif()
message("Using ${vsPath} libraries from ${CONAN_WIX_ROOT}/sdk/${vsPath}/lib/${arch}")

add_definitions(-D_USRDLL -DUNICODE -D_CONSOLE -D_UNICODE)
include_directories("${CONAN_WIX_ROOT}/sdk/inc")
link_directories("${CONAN_WIX_ROOT}/sdk/${vsPath}/lib/${arch}")
add_compile_options(/MT)

foreach(sanitizer ${enabledSanitizers})
    foreach(flagList CMAKE_C_FLAGS CMAKE_CXX_FLAGS)
        string(REPLACE " /fsanitize=${sanitizer}" "" ${flagList} "${${flagList}}")
    endforeach()
endforeach()

nx_add_target(wix_custom_actions LIBRARY
    NO_MOC
    WERROR_IF LINUX
    RC_FILE "CustomActions.rc"
    ADDITIONAL_SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/CustomActions.def"
    PRIVATE_LIBS
        userenv
        Rpcrt4
        Ws2_32
        IPHLPAPI
        comsuppw
        msi
        dutil
        wcautil
        Version
    FOLDER distribution
)
set_property(TARGET wix_custom_actions PROPERTY CXX_STANDARD 20)
