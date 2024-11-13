## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

add_executable(push_notification_extension
    "notification_service.h"
    "notification_service.mm")

if(NOT CMAKE_GENERATOR STREQUAL "Xcode")
    target_link_options(push_notification_extension PRIVATE "-e" "_NSExtensionMain")
endif()

set(entitlements_file "push_notification_extension.entitlements")
set(target_entitlements_file ${CMAKE_CURRENT_BINARY_DIR}/${entitlements_file})
nx_configure_file(${entitlements_file}.in ${target_entitlements_file})

add_dependencies(push_notification_extension utils)
set_target_properties(push_notification_extension PROPERTIES
    XCODE_PRODUCT_TYPE com.apple.product-type.app-extension
    MACOSX_BUNDLE TRUE
    BUNDLE TRUE
    BUNDLE_EXTENSION "appex"
    XCODE_ATTRIBUTE_WRAPPER_EXTENSION "appex"
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_LIST_DIR}/Info.plist"
    XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER
        "${customization.mobile.ios.bundleIdentifier}.extension"
    XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1,2"
    XCODE_ATTRIBUTE_CODE_SIGN_ENTITLEMENTS ${target_entitlements_file}
)

target_link_options(push_notification_extension PUBLIC "-ObjC")

target_link_libraries(push_notification_extension PRIVATE
    "-framework Foundation"
    "-framework UserNotifications"
    "-framework Security"
    "-framework MobileCoreServices"
    utils
)

add_subdirectory(utils)

if(CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    nx_process_macos_target_debug_symbols(
        push_notification_extension
        OUTPUT_DIR "${CMAKE_BINARY_DIR}")
endif()

nx_prepare_ios_signing(push_notification_extension
    "extension"
    ${customization.mobile.ios.bundleIdentifier}.extension)
