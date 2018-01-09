if(customization STREQUAL "hanwha")
    add_subdirectory(hanwha_metadata_plugin)
endif()

add_subdirectory(hikvision_metadata_plugin)

add_subdirectory(axis_metadata_plugin)

if(withPluginStubs)
    add_subdirectory(stub_metadata_plugin)
endif()

