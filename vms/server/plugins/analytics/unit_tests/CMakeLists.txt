## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

# Generate configuration file for the test - it contains the list of plugin library paths.

nx_add_server_plugin_test(analytics_plugin_ut NO_GTEST
    PRIVATE_LIBS
        nx_sdk
        nx_kit
)

if(LINUX)
    target_link_libraries(analytics_plugin_ut
        PRIVATE
            dl
            # The libpthread library must be explicitly linked, because it is used by libstdc++ via
            # weak symbols instead of being listed as a dependency. The option -pthread (which
            # implies -lpthread) is not enough here, because in some linkers the option --as-needed
            # is too agressive and optimizes away libpthread.
            -Wl,--no-as-needed pthread -Wl,--as-needed
    )
    # The option -pthread is needed in addition to linking with libpthread.
    set_target_properties(analytics_plugin_ut PROPERTIES LINK_FLAGS -pthread)
endif()

# Generate unit test config file with the plugin library list.

get_target_property(testBinDir analytics_plugin_ut RUNTIME_OUTPUT_DIRECTORY)

if(WINDOWS)
    set(libPrefix "")
    set(libSuffix ".dll")
else()
    set(libPrefix "lib")
    set(libSuffix ".so")
endif()

set(testConfigFile ${testBinDir}/analytics_plugin_ut.cfg)
string(JOIN "\n" testConfigContent
    "# Paths to plugin library files which must be unit-tested inside the VMS build directory."
    "plugins_optional/stub_analytics_plugin/${libPrefix}stub_analytics_plugin${libSuffix}"
    "plugins_optional/${libPrefix}sample_analytics_plugin${libSuffix}"
)
file(GENERATE OUTPUT ${testConfigFile} CONTENT ${testConfigContent})
nx_store_known_file(${testConfigFile})
