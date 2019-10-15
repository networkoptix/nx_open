if(NOT targetDevice STREQUAL "linux_arm64")
    # Use a multi-platform stub for lib tegra_video.

    set(tegraVideoWithTests ${withTests} CACHE STRING "" FORCE)

    add_subdirectory(
        ${CMAKE_SOURCE_DIR}/artifacts/tx1/tegra_multimedia_api
        ${CMAKE_BINARY_DIR}/artifacts/tx1/tegra_multimedia_api
    )

    if(withTests)
        add_dependencies(unit_tests tegra_video_ut)
    endif()

    set_target_properties(tegra_video PROPERTIES FOLDER third_party)

    if(TARGET tegra_video_ut)
         set_target_properties(tegra_video_ut PROPERTIES FOLDER third_party)
    endif()

    if(TARGET tegra_video_EXCLUDED)
         set_target_properties(tegra_video_EXCLUDED PROPERTIES FOLDER third_party)
    endif()
endif()
