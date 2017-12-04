if(customization STREQUAL "hanwha")
    add_subdirectory(hanwha_metadata_plugin)
endif()

if(withPluginStubs)
    add_subdirectory(stub_metadata_plugin)
endif()

