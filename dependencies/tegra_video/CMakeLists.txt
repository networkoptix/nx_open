if(NOT targetDevice STREQUAL "tx1")
    # Use a multi-platform stub for lib tegra_video.

    set(tegraVideoWithTests ${withTests} CACHE STRING "" FORCE)

    add_subdirectory(
        ${CMAKE_SOURCE_DIR}/artifacts/tx1/tegra_multimedia_api
        ${CMAKE_BINARY_DIR}/artifacts/tx1/tegra_multimedia_api
    )

    if(withTests)
        add_dependencies(unit_tests tegra_video_ut)
    endif()
endif()
