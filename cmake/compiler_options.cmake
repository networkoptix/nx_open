add_definitions(
    -DUSE_NX_HTTP
    -D__STDC_CONSTANT_MACROS
    -DENABLE_SSL
    -DENABLE_SENDMAIL
    -DENABLE_DATA_PROVIDERS
    -DENABLE_SOFTWARE_MOTION_DETECTION
    -DENABLE_THIRD_PARTY
    -DENABLE_MDNS
)

if(WIN32)
    add_definitions(
        -DENABLE_VMAX
        -DENABLE_DESKTOP_CAMERA
    )
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
        -DENABLE_MDNS
    )

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
        -DENABLE_PULSE_CAMERA
    )
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
    add_definitions(
        -std=c++1y
        -Werror=enum-compare
        -Werror=reorder
        -Werror=delete-non-virtual-dtor
        -Werror=return-type
        -Werror=conversion-null
        -Wuninitialized
    )

    if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
        add_definitions(
            -Wno-c++14-extensions
            -Wno-inconsistent-missing-override
        )
    endif()
endif()

if(LINUX)
    if(NOT "${arch}" STREQUAL "arm")
        add_definitions(-msse2)
    endif()
    add_definitions(
        -Wno-unknown-pragmas
        -Wno-ignored-qualifiers
    )
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
