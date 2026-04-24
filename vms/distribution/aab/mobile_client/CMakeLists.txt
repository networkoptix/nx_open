## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

set(android_abis arm64)

nx_configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/build_distribution.conf.in
    ${CMAKE_CURRENT_BINARY_DIR}/build_distribution.conf
    @ONLY)

set(script ${CMAKE_CURRENT_SOURCE_DIR}/build_distribution.sh)
if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin")
    # Bash in macOS is no longer updated and does not support some features. However Zsh works well
    # in our case.
    set(shell /bin/zsh)
else()
    set(shell /bin/bash)
endif()
add_custom_target(distribution_aab_mobile_client ALL
    COMMAND ${shell} ${script}
    DEPENDS ${script} mobile_client_apk
    COMMENT "Creating Android application bundle for mobile client"
)
