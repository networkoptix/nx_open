## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

cmake_minimum_required(VERSION 3.14)
project(analytics_plugin_ut)

# Unit test which tests an arbitrary list of Analytics Plugins.
#
# The list of plugin dynamic libraries must be specified in analytics_plugin_ut.cfg located next
# to the unit test executable.
#
# NOTE: To simplify the build system, unit tests of nx_kit and the SDK core library are built and
# run in the scope of this project.

#--------------------------------------------------------------------------------------------------
# Define analytics_plugin_ut executable.

file(GLOB_RECURSE ANALYTICS_PLUGIN_UNIT_TESTS_SRC CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_LIST_DIR}/src/*)

add_executable(analytics_plugin_ut ${ANALYTICS_PLUGIN_UNIT_TESTS_SRC})

target_link_libraries(analytics_plugin_ut PRIVATE nx_kit nx_sdk)
if(NOT WIN32)
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

target_compile_definitions(analytics_plugin_ut PRIVATE NX_SDK_API=) #< for nxLibContext()

add_test(NAME analytics_plugin_ut COMMAND analytics_plugin_ut)

# Generate a config file with the plugin library list.

set(testConfigFileHeader "# Paths to plugin library files which must be unit-tested.")

if(WIN32)
    set(libPrefix "")
    set(libSuffix "dll")
else()
    set(libPrefix "lib")
    set(libSuffix "so")
endif()

set(testConfigFile ${CMAKE_CURRENT_BINARY_DIR}/analytics_plugin_ut.cfg)
string(CONCAT testConfigContent
    "${testConfigFileHeader}\n"
    "../stub_analytics_plugin/${libPrefix}stub_analytics_plugin.${libSuffix}\n"
    "../sample_analytics_plugin/${libPrefix}sample_analytics_plugin.${libSuffix}\n"
)
file(GENERATE OUTPUT ${testConfigFile} CONTENT ${testConfigContent})
