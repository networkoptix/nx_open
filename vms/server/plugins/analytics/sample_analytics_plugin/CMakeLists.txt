## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

# Used in the Combined_plugin.
nx_add_target(sample_analytics_plugin_integration LIBRARY
    LIBRARY_TYPE STATIC
    NO_MOC
    WERROR_IF LINUX
    PUBLIC_LIBS nx_sdk
    FOLDER server/libs
)

nx_add_server_plugin(sample_analytics_plugin NO_MOC OPTIONAL
    SOURCE_DIR "plugin"
    PRIVATE_LIBS sample_analytics_plugin_integration
)
