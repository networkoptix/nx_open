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

target_include_directories(nx_media_core
    PRIVATE
        ${Qt6Core_PRIVATE_INCLUDE_DIRS}
)

if(withTests)
    add_subdirectory(unit_tests)
endif()
