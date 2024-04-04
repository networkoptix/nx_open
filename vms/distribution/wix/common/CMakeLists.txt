## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

include(wix_utils)

nx_wix_candle(check_architecture
    EXTENSIONS
        WixUtilExtension
        WixBalExtension
    STATIC)

nx_wix_candle(common_qt_libraries)
nx_wix_candle(common_qt_platforms)
nx_wix_candle(icu_libraries)
nx_wix_candle(vcrt_libraries)
nx_wix_candle(ucrt_libraries)
nx_wix_candle(openssl_libraries)
nx_wix_candle(ffmpeg_libraries)
nx_wix_candle(common_nx_libraries)
nx_wix_candle(metadata)
nx_wix_candle(asan_libraries VARIABLES asan_library_name=${asan_library_name})

add_custom_target(distribution_wix_common_resources
    DEPENDS
        ${CMAKE_CURRENT_BINARY_DIR}/check_architecture.wixobj
        ${CMAKE_CURRENT_BINARY_DIR}/common_nx_libraries.wixobj
        ${CMAKE_CURRENT_BINARY_DIR}/common_qt_libraries.wixobj
        ${CMAKE_CURRENT_BINARY_DIR}/common_qt_platforms.wixobj
        ${CMAKE_CURRENT_BINARY_DIR}/icu_libraries.wixobj
        ${CMAKE_CURRENT_BINARY_DIR}/metadata.wixobj
        ${CMAKE_CURRENT_BINARY_DIR}/vcrt_libraries.wixobj
        ${CMAKE_CURRENT_BINARY_DIR}/ucrt_libraries.wixobj
        ${CMAKE_CURRENT_BINARY_DIR}/openssl_libraries.wixobj
        ${CMAKE_CURRENT_BINARY_DIR}/ffmpeg_libraries.wixobj
        ${CMAKE_CURRENT_BINARY_DIR}/asan_libraries.wixobj
    COMMENT "[Wix] Generating common resources"
)
set_target_properties(distribution_wix_common_resources PROPERTIES FOLDER distribution)
