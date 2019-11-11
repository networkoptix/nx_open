nx_wix_heat(distribution_wix_desktop_client_webengine_resources
    SOURCE_DIR ${qt.dir}/resources
    TARGET_DIR_ALIAS resources
    COMPONENT_GROUP webengine_resources)

add_custom_target(distribution_wix_desktop_client_resources
    DEPENDS
        distribution_wix_desktop_client_webengine_resources
    COMMENT "[Wix] Generating client resources"
)
set_target_properties(distribution_wix_desktop_client_resources PROPERTIES FOLDER distribution)
