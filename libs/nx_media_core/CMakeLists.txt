## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

nx_add_target(nx_media_core LIBRARY
    PUBLIC_LIBS
        Qt6::Core
        nx_utils
        nx_codec
        nx_metric
        nx_reflect
        nx_fusion
        ffmpeg::ffmpeg
    PRIVATE_LIBS
        Qt6::Gui
    FOLDER common/libs
)

target_compile_definitions(nx_media_core
    PRIVATE NX_MEDIA_CORE_API=${API_EXPORT_MACRO}
    INTERFACE NX_MEDIA_CORE_API=${API_IMPORT_MACRO}
)

if("${arch}" STREQUAL "arm" OR "${arch}" STREQUAL "arm64")
    find_package(sse2neon)
    target_link_libraries(nx_media_core PUBLIC sse2neon::sse2neon)
endif()

include(CheckCXXCompilerFlag)

check_cxx_compiler_flag("-march=x86-64-v3" COMPILER_SUPPORTS_X86_64_V3)

if(COMPILER_SUPPORTS_X86_64_V3)
    target_compile_options(nx_media_core PRIVATE -march=x86-64-v3)
endif()

target_include_directories(nx_media_core
    PRIVATE
        ${Qt6Core_PRIVATE_INCLUDE_DIRS}
)

if(withTests)
    add_subdirectory(unit_tests)
endif()
