nx_wix_heat(distribution_wix_desktop_client_webengine_resources
    SOURCE_DIR ${qt.dir}/resources
    TARGET_DIR_ALIAS resources
    COMPONENT_GROUP webengine_resources)

nx_wix_heat(distribution_wix_desktop_client_webengine_locales
    SOURCE_DIR ${qt.dir}/translations/qtwebengine_locales
    TARGET_DIR_ALIAS qtwebengine_locales
    COMPONENT_GROUP webengine_locales)

add_custom_target(distribution_wix_desktop_client_resources
    DEPENDS
        distribution_wix_desktop_client_webengine_resources
        distribution_wix_desktop_client_webengine_locales
    COMMENT "[Wix] Generating client resources"
)
set_target_properties(distribution_wix_desktop_client_resources PROPERTIES FOLDER distribution)
