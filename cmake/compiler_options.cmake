set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(developerBuild)
    set(CMAKE_LINK_DEPENDS_NO_SHARED ON)
endif()

if(CMAKE_BUILD_TYPE MATCHES "Release|RelWithDebInfo")
    # TODO: Use CMake defaults in the next release version (remove the following two lines).
    string(REPLACE "-O3" "-O2" CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE}")
    string(REPLACE "-O3" "-O2" CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")

    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        add_compile_options(-fno-devirtualize)
    endif()
endif()

if(developerBuild)
    set(CMAKE_WIN32_EXECUTABLE OFF)
else()
    set(CMAKE_WIN32_EXECUTABLE ON)
endif()

add_definitions(
    -DUSE_NX_HTTP
    -D__STDC_CONSTANT_MACROS
    -DENABLE_SSL
    -DENABLE_SENDMAIL
    -DENABLE_DATA_PROVIDERS
    -DBOOST_BIND_NO_PLACEHOLDERS
    -D_SILENCE_CXX17_ITERATOR_BASE_CLASS_DEPRECATION_WARNING
)

set(enableSpeechSynthesizer "true")
set(isEdgeServer "false")

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set(enabledSanitizers "" CACHE STRING "A semicolon-separated list of enabled sanitizers")

    foreach(sanitizer ${enabledSanitizers})
        add_compile_options(-fsanitize=${sanitizer})

        if(sanitizer STREQUAL "address")
            string(APPEND CMAKE_SHARED_LINKER_FLAGS " -lasan")
            string(APPEND CMAKE_EXE_LINKER_FLAGS " -lasan")
        endif()
    endforeach()
endif()

if(WINDOWS)
    add_definitions(
        -D_CRT_RAND_S
        -D_WINSOCKAPI_=
    )
endif()

if(ANDROID OR IOS)
    remove_definitions(
        -DENABLE_SENDMAIL
    )
endif()

if(box MATCHES "isd|edge1")
    set(enableAllVendors OFF)
    set(isEdgeServer "true")
    set(enableSpeechSynthesizer "false")
endif()

if(WINDOWS)
    set(API_IMPORT_MACRO "__declspec(dllimport)")
    set(API_EXPORT_MACRO "__declspec(dllexport)")
else()
    set(API_IMPORT_MACRO "")
    set(API_EXPORT_MACRO "") #< TODO: Consider using "__attribute__((visibility(\"default\")))".
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    if(NOT WINDOWS)
        add_definitions(-D_DEBUG)
    endif()
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra)
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

        # Deletion of pointer to incomplete type 'X'; no destructor called.
        /we4150

        # Not all control paths return a value.
        /we4715

        # Macro redefinition.
        /we4005

        # Unsafe operation: no value of type 'INTEGRAL' promoted to type 'ENUM' can equal the given
        # constant.
        /we4806

        # 'identifier' : type name first seen using 'objecttype1' now seen using 'objecttype2'
        /we4099

        # Wrong initialization order.
        /we5038
    )
    add_definitions(-D_SILENCE_CXX17_OLD_ALLOCATOR_MEMBERS_DEPRECATION_WARNING)
    add_definitions(-D_SILENCE_CXX17_ALLOCATOR_VOID_DEPRECATION_WARNING)

    # Get rid of useless MSVC warnings.
    add_definitions(
        -D_CRT_SECURE_NO_WARNINGS #< Don't warn for deprecated 'unsecure' CRT functions.
        -D_CRT_NONSTDC_NO_DEPRECATE #< Don't warn for deprecated POSIX functions.
        -D_SCL_SECURE_NO_WARNINGS #< Don't warn for 'unsafe' STL functions.
    )

    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_compile_options(/wd4250)
    endif()

    set(_extra_linker_flags "/LARGEADDRESSAWARE /OPT:NOREF /ignore:4221")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${_extra_linker_flags} /entry:mainCRTStartup")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ${_extra_linker_flags}")
    set(CMAKE_STATIC_LINKER_FLAGS "${CMAKE_STATIC_LINKER_FLAGS} /ignore:4221")

    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /Zi")
    set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} /DEBUG")
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /DEBUG")

    set(CMAKE_SHARED_LINKER_FLAGS_DEBUG "${CMAKE_SHARED_LINKER_FLAGS_DEBUG} /DEBUG")
    set(CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG} /DEBUG")
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
        add_compile_options(
            -Wno-error=maybe-uninitialized
            -Wno-psabi
        )
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        add_compile_options(
            -Wno-c++14-extensions
            -Wno-inconsistent-missing-override
            -Werror=mismatched-tags
        )
    endif()
endif()

if(LINUX)
    if("${arch}" STREQUAL "x86" OR "${arch}" STREQUAL "x64")
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

    string(APPEND CMAKE_EXE_LINKER_FLAGS " -Wl,--disable-new-dtags")
    string(APPEND CMAKE_SHARED_LINKER_FLAGS " -rdynamic -Wl,--no-undefined")
    string(APPEND CMAKE_EXE_LINKER_FLAGS " -Wl,--as-needed")
    string(APPEND CMAKE_SHARED_LINKER_FLAGS " -Wl,--as-needed")

    if(NOT ANDROID AND CMAKE_BUILD_TYPE STREQUAL "Release")
        add_compile_options(-ggdb1 -fno-omit-frame-pointer)
    endif()
endif()

if(MACOSX)
    add_compile_options(
        -msse4.1
        -Wno-unused-local-typedef
    )
    set(CMAKE_INSTALL_RPATH @executable_path/../lib)
    set(CMAKE_SKIP_BUILD_RPATH ON)
    set(CMAKE_BUILD_WITH_INSTALL_RPATH ON)
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
if(targetDevice MATCHES "bpi|linux_arm32|edge1"
    OR targetDevice STREQUAL "linux-x64"
    OR targetDevice STREQUAL "linux-x86"
    OR (targetDevice STREQUAL "" AND platform STREQUAL "linux")
)
    set(strip_binaries OFF)
endif()

option(stripBinaries "Strip the resulting binaries" ${strip_binaries})
