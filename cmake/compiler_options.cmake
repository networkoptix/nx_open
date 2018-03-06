set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    # Visual Studio 2017 currently (15.5.x) ignores "set(CMAKE_CXX_STANDARD 17)".
    # So we need to set c++17 support explicitly.
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /std:c++17")
endif(MSVC)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(developerBuild)
    set(CMAKE_LINK_DEPENDS_NO_SHARED ON)
endif()

option(analyzeMutexLocksForDeadlock
    "Analyze mutex locks for deadlock. WARNING: this can significantly reduce performance!"
    OFF)

if(MSVC)
    # MSVC does not support compiler feature detection macros, so Qt fails to enable constexpr
    # for some its claasses like QRect, QMargins, etc.
    if(MSVC_VERSION GREATER 1900)
        add_definitions(-D__cpp_constexpr=201304)
    else()
        add_definitions(-DQ_COMPILER_CONSTEXPR)
    endif()
endif()

if(CMAKE_BUILD_TYPE MATCHES "Release|RelWithDebInfo")
    # TODO: Use CMake defaults in the next release version (remove the following two lines).
    string(REPLACE "-O3" "-O2" CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE}")
    string(REPLACE "-O3" "-O2" CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")

    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        add_compile_options(-fno-devirtualize)
    endif()
endif()

add_definitions(
    -DUSE_NX_HTTP
    -D__STDC_CONSTANT_MACROS
    -DENABLE_SSL
    -DENABLE_SENDMAIL
    -DENABLE_DATA_PROVIDERS
    -DENABLE_SOFTWARE_MOTION_DETECTION
)

if(WINDOWS)
    add_definitions(
        -D_WINSOCKAPI_=
    )
endif()

if(ANDROID OR IOS)
    remove_definitions(
        -DENABLE_SENDMAIL
        -DENABLE_SOFTWARE_MOTION_DETECTION
    )
endif()

if(box MATCHES "isd")
    set(enableAllVendors OFF)
    remove_definitions(-DENABLE_MOTION_DETECTION)
    add_definitions(-DEDGE_SERVER)
endif()

if(box MATCHES "edge1")
    set(enableAllVendors OFF)
    add_definitions(-DEDGE_SERVER)
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

    add_compile_options(
        /MP
        /bigobj
        /wd4290
        /wd4661
        /wd4100
        /we4717
    )
    add_definitions(-D_SILENCE_CXX17_OLD_ALLOCATOR_MEMBERS_DEPRECATION_WARNING)
    add_definitions(-D_SILENCE_CXX17_ALLOCATOR_VOID_DEPRECATION_WARNING)

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

    set(CMAKE_SHARED_LINKER_FLAGS_DEBUG "${CMAKE_SHARED_LINKER_FLAGS_DEBUG} /DEBUG:FASTLINK")
    set(CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG} /DEBUG:FASTLINK")
    if(NOT developerBuild)
        set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /SUBSYSTEM:WINDOWS /entry:mainCRTStartup")
    endif()
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

    if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        add_compile_options(
            -Wno-c++14-extensions
            -Wno-inconsistent-missing-override
        )
    endif()
endif()

if(LINUX)
    if(NOT "${arch}" MATCHES "arm|aarch64")
        add_compile_options(-msse2)
    endif()
    add_compile_options(
        -Wno-unknown-pragmas
        -Wno-ignored-qualifiers
        -fstack-protector-all #< TODO: Use -fstask-protector-strong when supported.
    )
    set(CMAKE_SKIP_BUILD_RPATH ON)
    set(CMAKE_BUILD_WITH_INSTALL_RPATH ON)
    set(CMAKE_INSTALL_RPATH "$ORIGIN/../lib")

    set(CMAKE_EXE_LINKER_FLAGS
        "${CMAKE_EXE_LINKER_FLAGS} -Wl,--disable-new-dtags")
    set(CMAKE_SHARED_LINKER_FLAGS
        "${CMAKE_SHARED_LINKER_FLAGS} -rdynamic -Wl,--no-undefined")

    if(NOT ANDROID AND CMAKE_BUILD_TYPE STREQUAL "Release")
        add_compile_options(-ggdb1 -fno-omit-frame-pointer)
    endif()
endif()

if(MACOSX)
    add_compile_options(
        -msse4.1
        -Wno-unused-local-typedef
    )
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
