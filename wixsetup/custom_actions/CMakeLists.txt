project(WixCustomActions)

if (NOT CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    message(FATAL_ERROR "Unsupported compiler ${CMAKE_CXX_COMPILER_ID}")
endif()

set(vsPath "vs2015")
if (CMAKE_CXX_COMPILER_VERSION MATCHES "19.*")
    set(vsPath "vs2017")
endif()
message("Using ${vsPath} libraries from ${wix_directory}/sdk/${vsPath}/lib/${arch}")

add_definitions(-D_USRDLL -DUNICODE -D_CONSOLE -D_UNICODE)
include_directories("${wix_directory}/sdk/inc")
link_directories("${wix_directory}/sdk/${vsPath}/lib/${arch}")
add_compile_options(/MT)

nx_add_target(wix_custom_actions LIBRARY LIBRARY_TYPE SHARED
    NO_PCH
    NO_MOC
    ADDITIONAL_SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/CustomActions.rc"
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
)
