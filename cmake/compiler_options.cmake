set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(CMAKE_LINK_DEPENDS_NO_SHARED ON)

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

if(WINDOWS)
    add_definitions(
        -D_WINSOCKAPI_=
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
        -DENABLE_SENDMAIL
        -DENABLE_DATA_PROVIDERS
        -DENABLE_SOFTWARE_MOTION_DETECTION
        -DENABLE_THIRD_PARTY
        -DENABLE_MDNS
    )
    set(enableAllVendors OFF)
endif()

if(box MATCHES "isd|edge1")
    set(enableAllVendors OFF)
    remove_definitions(-DENABLE_SOFTWARE_MOTION_DETECTION)
    add_definitions(-DEDGE_SERVER)
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
        -DENABLE_FLIR
        -DENABLE_ADVANTECH
    )
endif()

if(WINDOWS)
    set(API_IMPORT_MACRO "__declspec(dllimport)")
    set(API_EXPORT_MACRO "__declspec(dllexport)")
else()
    set(API_IMPORT_MACRO "")
    set(API_EXPORT_MACRO "")
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    if(NOT WINDOWS)
        add_definitions(-D_DEBUG)
    endif()
    add_definitions(-DUSE_OWN_MUTEX)
    if(analyzeMutexLocksForDeadlock)
        add_definitions(-DANALYZE_MUTEX_LOCKS_FOR_DEADLOCK)
    endif()
endif()

if(WINDOWS)
    add_definitions(
        -DNOMINMAX=
        -DUNICODE)
    set_property(DIRECTORY APPEND PROPERTY COMPILE_DEFINITIONS
        $<$<STREQUAL:$<TARGET_PROPERTY:TYPE>,STATIC_LIBRARY>:QN_EXPORT=>
        $<$<NOT:$<STREQUAL:$<TARGET_PROPERTY:TYPE>,STATIC_LIBRARY>>:QN_EXPORT=Q_DECL_EXPORT>)

    add_compile_options(
        /MP
        /bigobj
        /wd4290
        /wd4661
        /wd4100
        /we4717
    )

    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_compile_options(/wd4250)
    endif()

    set(_extra_linker_flags "/LARGEADDRESSAWARE /OPT:NOREF /ignore:4221")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${_extra_linker_flags}")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ${_extra_linker_flags}")
    set(CMAKE_STATIC_LINKER_FLAGS "${CMAKE_STATIC_LINKER_FLAGS} /ignore:4221")

    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /Zi")
    set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} /DEBUG")
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /DEBUG")
    unset(_extra_linker_flags)
endif()

if(UNIX)
    add_compile_options(
        -Werror=enum-compare
        -Werror=reorder
        -Werror=delete-non-virtual-dtor
        -Werror=return-type
        -Werror=conversion-null
        -Wuninitialized
        -Wno-error=unused-function
    )

    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 7.0)
            add_compile_options(-Wno-error=dangling-else)
        endif()
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        add_compile_options(
            -Wno-c++14-extensions
            -Wno-inconsistent-missing-override
        )
    endif()
endif()

if(LINUX)
    # TODO: Use CMake defaults in the next release version (remove the following two lines).
    string(REPLACE "-O3" "-O2" CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE}")
    string(REPLACE "-O3" "-O2" CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")

    if(NOT "${arch}" STREQUAL "arm")
        add_compile_options(-msse2)
    endif()
    add_compile_options(
        -Wno-unknown-pragmas
        -Wno-ignored-qualifiers
    )
    set(CMAKE_SKIP_BUILD_RPATH ON)
    set(CMAKE_BUILD_WITH_INSTALL_RPATH ON)

    if(LINUX)
        set(CMAKE_INSTALL_RPATH "$ORIGIN/../lib")
    endif()

    set(CMAKE_EXE_LINKER_FLAGS
        "${CMAKE_EXE_LINKER_FLAGS} -Wl,--disable-new-dtags")
    set(CMAKE_SHARED_LINKER_FLAGS
        "${CMAKE_SHARED_LINKER_FLAGS} -rdynamic -Wl,--allow-shlib-undefined")

    if(NOT ANDROID AND CMAKE_BUILD_TYPE STREQUAL "Release")
        add_compile_options(-ggdb1 -fno-omit-frame-pointer)
    endif()
endif()

if(MACOSX)
    add_compile_options(
        -msse4.1
        -Wno-unused-local-typedef
    )
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -undefined dynamic_lookup")
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(_qml_debug ON)
else()
    set(_qml_debug OFF)
endif()

option(qml_debug "Enable QML debugger" ${_qml_debug})
unset(_qml_debug)
if(qml_debug)
    add_definitions(-DQT_QML_DEBUG)
endif()

set(strip_binaries ON)
if(targetDevice MATCHES "bpi|bananapi|rpi|edge1"
    OR targetDevice STREQUAL "linux-x64"
    OR targetDevice STREQUAL "linux-x86"
    OR (targetDevice STREQUAL "" AND platform STREQUAL "linux")
)
    set(strip_binaries OFF)
endif()

option(stripBinaries "Strip the resulting binaries" ${strip_binaries})
