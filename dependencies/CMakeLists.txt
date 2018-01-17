add_subdirectory(nx_kit)

if(NOT ANDROID AND NOT IOS)
    add_subdirectory(quazip)
endif()

add_subdirectory(tegra_video)
