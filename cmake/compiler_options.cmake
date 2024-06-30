## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_OBJCXX_STANDARD ${CMAKE_CXX_STANDARD})
set(CMAKE_OBJCXX_STANDARD_REQUIRED ON)
set(CMAKE_OBJCXX_EXTENSIONS OFF)

set(CMAKE_CUDA_STANDARD 11)
set(CMAKE_CUDA_STANDARD_REQUIRED ON)
set(CMAKE_CUDA_ARCHITECTURES 50)

set(CMAKE_CXX_VISIBILITY_PRESET hidden)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(CMAKE_LINK_DEPENDS_NO_SHARED ON)

if(LINUX AND NOT ANDROID)
    option(useLdGold "Use ld.gold to link binaries" OFF)
    option(verboseDebugInfoInRelease
        "Add more debug info when building Release version; effective only when -DuseClang=ON" OFF)
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
    set(CMAKE_WIN32_EXECUTABLE OFF) #< On Windows, build executables as console apps.
else()
    set(CMAKE_WIN32_EXECUTABLE ON) #< On Windows, build executables as GUI (non-console) apps.
endif()

add_definitions(
    -DUSE_NX_HTTP
    -D__STDC_CONSTANT_MACROS
    -DENABLE_SSL
    -DENABLE_SENDMAIL
    -DBOOST_BIND_NO_PLACEHOLDERS
    -D_SILENCE_CXX17_ITERATOR_BASE_CLASS_DEPRECATION_WARNING
    -DQT_NO_EXCEPTIONS
)

set(enableSpeechSynthesizer "true")

set(enabledSanitizers "" CACHE STRING "A semicolon-separated list of enabled sanitizers")

if(NOT "${enabledSanitizers}" STREQUAL "" AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_compile_options(
        "-fsanitize-blacklist=${PROJECT_SOURCE_DIR}/sanitize-blacklist.txt")
endif()

foreach(sanitizer ${enabledSanitizers})
    if(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
        get_filename_component(cl_dir ${CMAKE_CXX_COMPILER} DIRECTORY)
        get_filename_component(cl_lib_dir "${cl_dir}/../../../lib/x64" ABSOLUTE)
        file(TO_NATIVE_PATH ${cl_lib_dir} cl_lib_dir)

        if(sanitizer STREQUAL "address")
            # Select the proper build type of ASan library, because mixing /MD and /MDd produces false positives.
            if(CMAKE_BUILD_TYPE STREQUAL "Debug")
                set(asan_name_prefix "clang_rt.asan_dbg_dynamic")
            else()
                set(asan_name_prefix "clang_rt.asan_dynamic")
            endif()

            set(asan_library_name "${asan_name_prefix}-x86_64.dll")
            set(asan_library_path "${cl_dir}/${asan_library_name}")

            # Copy ASan DLL to bin/ for convenience.
            nx_copy("${asan_library_path}" DESTINATION "${PROJECT_BINARY_DIR}/bin")

            set(cl_link_flags /fsanitize=${sanitizer})
            add_compile_options(/fsanitize=${sanitizer})
            # Disable warning:
            #   C5059: runtime checks and address sanitizer is not currently supported - disabling runtime checks
            add_compile_options(/wd5059)
            # Disable the warning about missing debug info when compiled in release mode.
            add_compile_options(/wd5072)

            string(TOUPPER ${CMAKE_BUILD_TYPE} config)
            foreach(flag_var
                CMAKE_C_FLAGS
                CMAKE_C_FLAGS_${config}
                CMAKE_CXX_FLAGS
                CMAKE_CXX_FLAGS_${config}
            )
                string(REGEX REPLACE "[/|-]RTC(su|[1su])" "" ${flag_var} "${${flag_var}}")
            endforeach()

            string(APPEND CMAKE_SHARED_LINKER_FLAGS " ${cl_link_flags}")
            string(APPEND CMAKE_EXE_LINKER_FLAGS " ${cl_link_flags}")
        endif()
    else()
        add_compile_options(-fsanitize=${sanitizer})
        string(APPEND CMAKE_SHARED_LINKER_FLAGS " -fsanitize=${sanitizer}")
        string(APPEND CMAKE_EXE_LINKER_FLAGS " -fsanitize=${sanitizer}")

        if(sanitizer STREQUAL "address")
            add_compile_options(-fno-omit-frame-pointer)
            if(MACOSX)
                execute_process(COMMAND ${CMAKE_CXX_COMPILER}
                    -print-file-name=lib
                    OUTPUT_VARIABLE clang_lib_dir
                    OUTPUT_STRIP_TRAILING_WHITESPACE)

                set(asan_library_name "libclang_rt.asan_osx_dynamic.dylib")
                set(asan_library_path "${clang_lib_dir}/darwin/${asan_library_name}")

                nx_copy("${asan_library_path}" DESTINATION "${PROJECT_BINARY_DIR}/lib")
            endif()
        endif()

        if(sanitizer STREQUAL "undefined")
            string(APPEND CMAKE_EXE_LINKER_FLAGS " -lubsan")
        endif()
    endif()
endforeach()

set(enableCoverage OFF CACHE BOOL "Enable source-based code coverage")

if(enableCoverage)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        add_compile_options(-fprofile-instr-generate -fcoverage-mapping)
        add_link_options(-fprofile-instr-generate)
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
        add_compile_options(--coverage)
        add_link_options(--coverage)
    elseif(MSVC)
        add_link_options(/Profile)
    endif()
endif()

if(WINDOWS)
    add_definitions(
        -D_CRT_RAND_S
        -DNOMINMAX=
        -DUNICODE
        -DWIN32_LEAN_AND_MEAN
    )
endif()

if(ANDROID OR IOS)
    remove_definitions(
        -DENABLE_SENDMAIL
    )
endif()

if(targetDevice STREQUAL "edge1")
    set(enableAllVendors OFF)
    set(enableSpeechSynthesizer "false")
endif()

if(MSVC)
    set(API_IMPORT_MACRO "__declspec(dllimport)")
    set(API_EXPORT_MACRO "__declspec(dllexport)")
else()
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        set(API_IMPORT_MACRO "__attribute__((visibility(\"default\")))")
        set(API_EXPORT_MACRO "__attribute__((visibility(\"default\")))")
    else()
        set(API_IMPORT_MACRO "")
        set(API_EXPORT_MACRO "")
    endif()
endif()

# On GCC/Clang, when a class template in a shared library has a static member variable, such
# variable may produce duplicates (e.g. one in the library which "owns" the template, and one in a
# library that uses it) in case the default symbol visibility is set to "hidden". To avoid the
# issue, the following macro should be used when defining such static members, which raises the
# symbol visibility from "hidden" to "default".
#
# It is unnecessary for windows because the MSVC compiler correctly exports static members of
# a template if this template instance is exported.
#
# This macro doesn't prevent duplicates when the same class template instantiated in a different
# shared libraries.
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_definitions([[-DNX_FORCE_EXPORT=__attribute__((visibility("default")))]])
else()
    add_definitions(-DNX_FORCE_EXPORT=)
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    if(NOT WINDOWS)
        add_definitions(-D_DEBUG)
    endif()
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra)

    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        # Avoid multiple warnings with Singleton implementation.
        add_compile_options(-Wno-undefined-var-template)
    endif()
endif()

if(MSVC)
    add_compile_options(
        /MP
        /bigobj
        /Zc:inline

        /wd4290
        /wd4661
        /wd4100

        /we4717

        # Deletion of pointer to incomplete type 'X'; no destructor called.
        /we4150

        # Returning address of local variable or temporary.
        /we4172

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

        # operator '|': deprecated between enumerations of different types
        # Suppressed as it is used in rapidjson headers and thus generates tons of warnings.
        /wd5054
    )
    add_definitions(-D_SILENCE_CXX17_OLD_ALLOCATOR_MEMBERS_DEPRECATION_WARNING)
    add_definitions(-D_SILENCE_CXX17_ALLOCATOR_VOID_DEPRECATION_WARNING)

    # Get rid of useless MSVC warnings.
    add_definitions(
        -D_CRT_SECURE_NO_WARNINGS #< Don't warn for deprecated 'unsecure' CRT functions.
        -D_CRT_NONSTDC_NO_DEPRECATE #< Don't warn for deprecated POSIX functions.
        -D_SCL_SECURE_NO_WARNINGS #< Don't warn for 'unsafe' STL functions.
    )

    if(MSVC_VERSION LESS 1920)
        # Workaround for MSVC bug, see QTBUG-72073.
        add_definitions(-DQT_NO_FLOAT16_OPERATORS)
    endif()

    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_compile_options(/wd4250)

        # Avoid unnamed objects with custom construction and destruction. Suppressing this warning
        # as it is displayed very frequently in common places, e.g. in `QObject::connect` calls.
        add_compile_options(/wd26444)
    endif()

    set(_extra_linker_flags "/LARGEADDRESSAWARE /OPT:NOREF /ignore:4221")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${_extra_linker_flags} /entry:mainCRTStartup")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ${_extra_linker_flags}")
    set(CMAKE_STATIC_LINKER_FLAGS "${CMAKE_STATIC_LINKER_FLAGS} /ignore:4221")

    # Commented out for VideoCentrix, no debug info in release builds for now.
    # set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /Zi")
    # set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} /DEBUG")
    # set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /DEBUG")

    set(CMAKE_SHARED_LINKER_FLAGS_DEBUG "${CMAKE_SHARED_LINKER_FLAGS_DEBUG} /DEBUG")
    set(CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG} /DEBUG")
    unset(_extra_linker_flags)
endif()

if(UNIX)
    add_compile_options(
        -Werror=enum-compare
        -Werror=return-type
        -Wuninitialized
        -Wno-error=unused-function
    )

    string(APPEND CMAKE_CXX_FLAGS
        " -Werror=reorder -Werror=delete-non-virtual-dtor -Werror=conversion-null"
    )

    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        add_compile_options(
            -Wno-error=maybe-uninitialized
            -Wno-error=unused-result
            -Wno-error=ignored-attributes
            -Wno-error=deprecated-declarations
            -Wno-missing-field-initializers
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

set(CMAKE_SKIP_BUILD_RPATH ON)
set(CMAKE_BUILD_WITH_INSTALL_RPATH ON)

if(LINUX)
    if("${arch}" STREQUAL "x86" OR "${arch}" STREQUAL "x64")
        add_compile_options(-msse2)
    else()
        add_compile_options(-Wno-error=type-limits) #< Fix warnings from RapidJSON on ARM.
    endif()

    add_compile_options(
        -Wno-unknown-pragmas
        -Wno-ignored-qualifiers
        -fstack-protector-all #< TODO: Use -fstask-protector-strong when supported.
    )

    list(APPEND CMAKE_INSTALL_RPATH "$ORIGIN/../lib")

    # The two compilers that we use for building VMS project (gcc and clang) use different defaults:
    # gcc uses "--disable-new-dtags", while clang uses "--enable-new-dtags", so we have to specify
    # this option explicitly.
    string(APPEND CMAKE_EXE_LINKER_FLAGS " -Wl,--disable-new-dtags")
    string(APPEND CMAKE_SHARED_LINKER_FLAGS " -rdynamic -Wl,--disable-new-dtags")

    if(NOT (CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND NOT enabledSanitizers STREQUAL ""))
        string(APPEND CMAKE_SHARED_LINKER_FLAGS " -Wl,--no-undefined")
    else()
        message(STATUS "Disabling -Wl,--no-undefined since Clang sanitizer is not compatible with it")
    endif()

    set(link_flags " -Wl,--as-needed")

    if(useLdGold)
        list(APPEND link_flags " -fuse-ld=gold")
        list(APPEND link_flags " -Wl,--gdb-index")
    endif()

    string(APPEND CMAKE_EXE_LINKER_FLAGS ${link_flags})
    string(APPEND CMAKE_SHARED_LINKER_FLAGS ${link_flags})

    if(NOT ANDROID AND CMAKE_BUILD_TYPE STREQUAL "Release")
        if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND NOT verboseDebugInfoInRelease)
            set(debug_level_options "-gline-directives-only")
        else()
            set(debug_level_options "-ggdb1")
        endif()

        add_compile_options(-fno-omit-frame-pointer ${debug_level_options})
        string(APPEND CMAKE_SHARED_LINKER_FLAGS " -Wl,--compress-debug-sections=zlib")
        string(APPEND CMAKE_EXE_LINKER_FLAGS " -Wl,--compress-debug-sections=zlib")
    endif()
endif()

if(MACOSX)
    if("${arch}" STREQUAL "x64")
        add_compile_options(-msse4.1)
    endif()
    add_compile_options(-Wno-unused-local-typedef)
    list(APPEND CMAKE_INSTALL_RPATH @executable_path/../lib)
endif()

if(IOS)
    list(APPEND CMAKE_INSTALL_RPATH @executable_path/Frameworks)
endif()

if(IOS)
    # For some reason, the following warning is automatically enabled only on iOS build.
    add_compile_options(
        -Wno-shorten-64-to-32
    )
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(_qml_debug ON)
else()
    set(_qml_debug OFF)
endif()

if(CMAKE_BUILD_TYPE MATCHES "RelWithDebInfo" OR targetDevice STREQUAL "edge1")
    set(_stripBinaries ON)
else()
    set(_stripBinaries OFF)
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    option(compileTimeTracing "Enable compile time tracing (-ftime-trace)" OFF)
    if(compileTimeTracing)
        add_compile_options(-ftime-trace)
    endif()
endif()

option(qml_debug "Enable QML debugger" ${_qml_debug})
unset(_qml_debug)
if(qml_debug)
    add_definitions(-DQT_QML_DEBUG)
endif()

option(colorOutput "Always produce ANSI-colored output (GNU/Clang only)." OFF)

if(${colorOutput})
    if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
       add_compile_options(-fdiagnostics-color=always)
    elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
       add_compile_options(-fcolor-diagnostics)
    endif()
endif()
