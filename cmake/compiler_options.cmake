set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

add_definitions(
    -DUSE_NX_HTTP
    -D__STDC_CONSTANT_MACROS
    -DENABLE_SSL
    -DENABLE_SENDMAIL
    -DENABLE_DATA_PROVIDERS
    -DENABLE_SOFTWARE_MOTION_DETECTION
    -DENABLE_THIRD_PARTY
    -DENABLE_MDNS)

if(WIN32)
    add_definitions(
        -DENABLE_VMAX
        -DENABLE_DESKTOP_CAMERA    )
endif()

if(UNIX)
    add_definitions(-DQN_EXPORT=)
endif()

set(enableAllVendors ON)

if(ANDROID OR IOS)
    remove_definitions(
        -DENABLE_SSL
        -DENABLE_SENDMAIL
        -DENABLE_DATA_PROVIDERS
        -DENABLE_SOFTWARE_MOTION_DETECTION
        -DENABLE_THIRD_PARTY
        -DENABLE_MDNS)

    set(enableAllVendors OFF)
endif()

if(ISD OR ISD_S2)
    set(enableAllVendors OFF)
endif()

if(enableAllVendors)
    add_definitions(
        -DENABLE_ONVIF
        -DENABLE_AXIS
        -DENABLE_ACTI
        -DENABLE_ARECONT
        -DENABLE_DLINK
        -DENABLE_DROID
        -DENABLE_TEST_CAMERA
        -DENABLE_STARDOT
        -DENABLE_IQE
        -DENABLE_ISD
        -DENABLE_PULSE_CAMERA)
endif()

if(WIN32)
    set(API_IMPORT_MACRO "__declspec(dllimport)")
    set(API_EXPORT_MACRO "__declspec(dllexport)")
else()
    set(API_IMPORT_MACRO "")
    set(API_EXPORT_MACRO "")
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    if(NOT WIN32)
        add_definitions(-D_DEBUG)
    endif()
    add_definitions(-DUSE_OWN_MUTEX)
    if(analyzeMutexLocksForDeadlock)
        add_definitions(-DANALYZE_MUTEX_LOCKS_FOR_DEADLOCK)
    endif()
endif()

if(UNIX)
    add_compile_options(
        -Werror=enum-compare
        -Werror=reorder
        -Werror=delete-non-virtual-dtor
        -Werror=return-type
        -Werror=conversion-null
        -Wuninitialized)

    if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
        add_compile_options(
            -Wno-c++14-extensions
            -Wno-inconsistent-missing-override)
    endif()
endif()

if(LINUX)
    if(NOT "${arch}" STREQUAL "arm")
        add_compile_options(-msse2)
    endif()
    add_compile_options(
        -Wno-unknown-pragmas
        -Wno-ignored-qualifiers)

    if(fullRpath)
        set(CMAKE_SKIP_BUILD_RPATH OFF)
        set(CMAKE_BUILD_WITH_INSTALL_RPATH OFF)
    else()
        set(CMAKE_SKIP_BUILD_RPATH ON)
        set(CMAKE_BUILD_WITH_INSTALL_RPATH ON)
        set(CMAKE_INSTALL_RPATH "$ORIGIN/../lib")
    endif()

    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--disable-new-dtags")
    set(CMAKE_SHARED_LINKER_FLAGS
        "${CMAKE_SHARED_LINKER_FLAGS} -rdynamic -Wl,--allow-shlib-undefined")
endif()

if(MACOSX)
    add_compile_options(
        -msse4.1
        -Wno-unused-local-typedef)
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -undefined dynamic_lookup")
endif()

option(qml_debug "Enable QML debugger" ON)
if(qml_debug)
    add_definitions(-DQT_QML_DEBUG)
endif()

# set(CMAKE_AUTOMOC_MOC_OPTIONS "-bstdafx.h")
#
# if(WIN32)
#     set(platform "windows")
#     set(additional.compiler "msvc2012u3")
#     set(modification "winxp")
#     set(platformToolSet "v110_xp")
#     if(MSVC)
#         set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")
#         message(STATUS "Added parallel build arguments to CMAKE_CXX_FLAGS: ${CMAKE_CXX_FLAGS}")
#     endif()
# endif()
