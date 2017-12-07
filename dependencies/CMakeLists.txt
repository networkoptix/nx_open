if(NOT ANDROID AND NOT IOS)
    add_subdirectory(quazip)
endif()

if(NOT targetDevice STREQUAL "tx1")
    # Use a multi-platform stub for lib tegra_video.
    add_subdirectory(
        ${CMAKE_SOURCE_DIR}/artifacts/tx1/tegra_multimedia_api
        ${CMAKE_BINARY_DIR}/artifacts/tx1/tegra_multimedia_api
    )
endif()
