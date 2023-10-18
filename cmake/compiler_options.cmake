## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

# -------------------------------------------------------------------------------------------------
# CMake setup.

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)

set(CMAKE_OBJCXX_STANDARD ${CMAKE_CXX_STANDARD})
set(CMAKE_OBJCXX_STANDARD_REQUIRED ON)
set(CMAKE_OBJCXX_EXTENSIONS OFF)

set(CMAKE_CUDA_STANDARD 11)
set(CMAKE_CUDA_STANDARD_REQUIRED ON)
set(CMAKE_CUDA_ARCHITECTURES 50)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_LINK_DEPENDS_NO_SHARED ON)
set(CMAKE_SKIP_BUILD_RPATH ON)
set(CMAKE_BUILD_WITH_INSTALL_RPATH ON)

if(developerBuild)
    # Build all executables on Windows as console apps to simplify debug in developer mode.
    set(CMAKE_WIN32_EXECUTABLE OFF)
else()
    set(CMAKE_WIN32_EXECUTABLE ON)
endif()

# -------------------------------------------------------------------------------------------------
# Compiler detection.
# Currently we support 4 different compilers: MSVC, GNU GCC, Clang (*nix), Clang-Cl (win). Each of
# them has own set of flags and options. Here we define easy-to-use variables for each compiler.
#
# MSVC variable is ON for both MSVC compiler and Clang-Cl.
# compilerClang is ON for both native Clang and Clang-Cl.
set(compilerMsvc OFF)
set(compilerNativeClang OFF)
set(compilerClangCl OFF)
set(compilerClang OFF)
set(compilerGcc OFF)

if(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    set(compilerMsvc ON)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND MSVC)
    set(compilerClang ON)
    set(compilerClangCl ON)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set(compilerClang ON)
    set(compilerNativeClang ON)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    set(compilerGcc ON)
else()
    message(FATAL_ERROR "Unsupported compiler ${CMAKE_CXX_COMPILER_ID}")
endif()

# -------------------------------------------------------------------------------------------------
# Defines.

add_definitions(
    -D__STDC_CONSTANT_MACROS
    -DENABLE_SSL #< TODO: #sivanov Remove this one.
    -DBOOST_BIND_NO_PLACEHOLDERS
    -D_SILENCE_CXX17_ITERATOR_BASE_CLASS_DEPRECATION_WARNING
    -DQT_NO_EXCEPTIONS
)

# These definitions are specific for Windows headers.
if(WINDOWS)
    add_definitions(
        -D_CRT_RAND_S
        -D_WINSOCKAPI_=
        -DNOMINMAX=
        -DUNICODE
        -DWIN32_LEAN_AND_MEAN
    )
endif()

if(compilerClangCl)
    # Add definition to workaround some specific issues in codebase, e.g. too long include paths in
    # moc-generated files. Eventually these issues are to be resolved, so define will help us to
    # find all such workarounds quickly.
    add_definitions(-DNX_CLANG_CL)
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug" AND NOT MSVC)
    add_definitions(-D_DEBUG)
endif()

if(MSVC)
    set(API_IMPORT_MACRO "__declspec(dllimport)")
    set(API_EXPORT_MACRO "__declspec(dllexport)")
else()
    set(API_IMPORT_MACRO [[__attribute__((visibility("default")))]])
    set(API_EXPORT_MACRO [[__attribute__((visibility("default")))]])
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
if(NOT compilerMsvc)
    add_definitions([[-DNX_FORCE_EXPORT=__attribute__((visibility("default")))]])
else()
    add_definitions(-DNX_FORCE_EXPORT=)
endif()

# -------------------------------------------------------------------------------------------------
# User-defined options.

if(LINUX AND NOT ANDROID)
    option(useLdGold "Use ld.gold to link binaries" OFF)
    option(verboseDebugInfoInRelease
        "Add more debug info when building Release version; effective only when -DuseClang=ON" OFF)
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

if(compilerClang)
    option(compileTimeTracing "Enable compile time tracing (-ftime-trace)" OFF)
    if(compileTimeTracing)
        add_compile_options(-ftime-trace)
    endif()
endif()

option(colorOutput "Always produce ANSI-colored output (GNU/Clang only)." OFF)
if(${colorOutput})
    if(compilerGcc)
       add_compile_options(-fdiagnostics-color=always)
    elseif(compilerClang)
       add_compile_options(-fcolor-diagnostics)
    endif()
endif()

set(enabledSanitizers "" CACHE STRING "A semicolon-separated list of enabled sanitizers")
if(NOT "${enabledSanitizers}" STREQUAL "" AND compilerClang)
    add_compile_options(
        "-fsanitize-blacklist=${PROJECT_SOURCE_DIR}/sanitize-blacklist.txt")
endif()

foreach(sanitizer ${enabledSanitizers})
    if(compilerMsvc)
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
    if(compilerClang)
        add_compile_options(-fprofile-instr-generate -fcoverage-mapping)
        add_link_options(-fprofile-instr-generate)
    elseif(compilerGcc)
        add_compile_options(--coverage)
        add_link_options(--coverage)
    elseif(compilerMsvc)
        add_link_options(/Profile)
    endif()
endif()

# -------------------------------------------------------------------------------------------------
# Compiler options.

if(compilerGcc AND CMAKE_BUILD_TYPE MATCHES "Release|RelWithDebInfo")
    # Workaround of a GCC compilation issue.
    string(REPLACE "-O3" "-O2" CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE}")
    string(REPLACE "-O3" "-O2" CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
endif()

if(compilerMsvc)
    add_compile_options(
        /MP
        /bigobj
        /Zc:inline
        /Zc:preprocessor

        # Enable most warnings by default.
        /W4

        # Unreferenced formal parameter.
        # Suppressed as it produces a lot of false positives in variadic template functions.
        /wd4100

        # Unreachable code.
        # Suppressed as it produces a lot of false positives in template functions.
        /wd4702

        # Function is recursive on all control paths, will cause runtime stack overflow.
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

        # Conversion from 'size_t' to 'int', possible loss of data.
        /wd4267

        # 'type': class 'type1' needs to have dll-interface to be used by clients of class 'type2'.
        /wd4251

        # non - DLL-interface class 'class_1' used as base for DLL-interface class 'class_2'
        /wd4275

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

        # Avoid the following warning when building gtest:
        # "um\winbase.h(9531,5): warning C5105: macro expansion producing 'defined' has undefined
        # behavior"
        add_compile_options(/wd5105)

        # Avoid unnamed objects with custom construction and destruction. Suppressing this warning
        # as it is displayed very frequently in common places, e.g. in `QObject::connect` calls.
        add_compile_options(/wd26444)
    endif()

    # "/Z7" flag tells the compiler to save the debug info to the .obj file instead of sending this
    # data to the process mspdbsrv.exe, which saves it to a .pdb file. This approach allows to
    # avoid possible interprocess locks, and thus improves the parallelability of the entire build
    # process. The .pdb files are generated during the linking process - the "/DEBUG" flag passed
    # to the linker is responsible for this.
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /Z7")
    set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} /DEBUG")
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /DEBUG")

    set(CMAKE_SHARED_LINKER_FLAGS_DEBUG "${CMAKE_SHARED_LINKER_FLAGS_DEBUG} /DEBUG")
    set(CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG} /DEBUG")
endif()

if(MSVC)
    set(_extra_linker_flags "/LARGEADDRESSAWARE /OPT:NOREF /ignore:4221")
    set(CMAKE_EXE_LINKER_FLAGS
        "${CMAKE_EXE_LINKER_FLAGS} ${_extra_linker_flags} /entry:mainCRTStartup")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ${_extra_linker_flags}")
    set(CMAKE_STATIC_LINKER_FLAGS "${CMAKE_STATIC_LINKER_FLAGS} /ignore:4221")
    unset(_extra_linker_flags)
endif()

if(NOT compilerMsvc)
    if(compilerClangCl)
        # Clang-cl tries to emulate and support most of the cl.exe options, sometimes overwriting
        # default clang behavior.
        add_compile_options(
            /bigobj
            /Zc:inline
            # /W4 expands to -Wall, while -Wall itself, if passed directly, will be expanded to
            # -Weverything "for compatibility reasons".
            /W4

            -fms-extensions
            -fms-compatibility
            -fms-compatibility-version=19.34.00000
            -msse4.1
        )

        # This block contains warnings which should be fixed and changed to errors instead.
        add_compile_options(
            -Wno-non-virtual-dtor
            -Wno-deprecated
            -Wno-deprecated-declarations
            -Wno-unused-parameter
            -Wno-unused-but-set-parameter
            -Wno-unused-exception-parameter
            -Wno-newline-eof
            -Wno-cast-align
            -Wno-cast-function-type
            -Wno-unreachable-code-break
            -Wno-implicit-fallthrough
            -Wno-range-loop-construct
            -Wno-range-loop-bind-reference
            -Wno-conditional-uninitialized
        )

        # Suppress some warnings which are not to be fixed in the nearest time.
        add_compile_options(
            -Wno-sign-compare
            -Wno-microsoft-cast
            -Wno-language-extension-token
            -Wno-covered-switch-default #< Contradicts with MSVC warning.
        )
    else() # GCC / Native CLang
        add_compile_options(
            -Wall
            -fstack-protector-all #< TODO: Use -fstask-protector-strong when supported.
        )

        if("${arch}" STREQUAL "x86" OR "${arch}" STREQUAL "x64")
            add_compile_options(-msse2)
        else()
            add_compile_options(-Wno-error=type-limits) #< Fix warnings from RapidJSON on ARM.
        endif()
    endif()

    add_compile_options(
        -Wextra
        -Werror=enum-compare
        -Werror=return-type
        -Wuninitialized
        -Wno-error=unused-function
        -Wno-unknown-pragmas
        -Wno-ignored-qualifiers
        -Werror=reorder
        -Werror=delete-non-virtual-dtor
        -Werror=conversion-null
    )

    if(compilerMsvc)
        add_compile_options(/utf-8)
    elseif(compilerGcc)
        add_compile_options(
            -Wno-error=maybe-uninitialized
            -Wno-missing-field-initializers
            -Wno-psabi
        )
        if(CMAKE_BUILD_TYPE MATCHES "Release|RelWithDebInfo")
            add_compile_options(-fno-devirtualize)
        endif()
    elseif(compilerClang)
        add_compile_options(
            -Wno-undefined-var-template #< Avoid multiple warnings with Singleton implementation.
            -Wno-c++14-extensions
            -Wno-inconsistent-missing-override
            -Werror=mismatched-tags
        )
    endif()
endif()

if(LINUX)
    # The two compilers that we use for building VMS project (gcc and clang) use different defaults:
    # gcc uses "--disable-new-dtags", while clang uses "--enable-new-dtags", so we have to specify
    # this option explicitly.
    string(APPEND CMAKE_EXE_LINKER_FLAGS " -Wl,--disable-new-dtags")
    string(APPEND CMAKE_SHARED_LINKER_FLAGS " -rdynamic -Wl,--disable-new-dtags")

    if(NOT (compilerClang AND NOT enabledSanitizers STREQUAL ""))
        string(APPEND CMAKE_SHARED_LINKER_FLAGS " -Wl,--no-undefined")
    else()
        message(STATUS
            "Disabling -Wl,--no-undefined since Clang sanitizer is not compatible with it")
    endif()

    set(link_flags " -Wl,--as-needed")

    if(useLdGold)
        list(APPEND link_flags " -fuse-ld=gold")
        list(APPEND link_flags " -Wl,--gdb-index")
    endif()

    string(APPEND CMAKE_EXE_LINKER_FLAGS ${link_flags})
    string(APPEND CMAKE_SHARED_LINKER_FLAGS ${link_flags})

    if(NOT ANDROID AND CMAKE_BUILD_TYPE STREQUAL "Release")
        if(compilerClang AND NOT verboseDebugInfoInRelease)
            set(debug_level_options "-gline-directives-only")
        else()
            set(debug_level_options "-ggdb1")
        endif()

        add_compile_options(-fno-omit-frame-pointer ${debug_level_options})
        string(APPEND CMAKE_SHARED_LINKER_FLAGS " -Wl,--compress-debug-sections=zlib")
        string(APPEND CMAKE_EXE_LINKER_FLAGS " -Wl,--compress-debug-sections=zlib")
    endif()
endif()

# -------------------------------------------------------------------------------------------------
# OS-dependent options.

if(LINUX)
    list(APPEND CMAKE_INSTALL_RPATH "$ORIGIN/../lib")
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
    # For some reason, the following warning is automatically enabled only on iOS build.
    add_compile_options(-Wno-shorten-64-to-32)
endif()
