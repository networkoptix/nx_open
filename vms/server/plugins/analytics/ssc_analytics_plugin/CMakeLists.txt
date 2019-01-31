find_package(Qt5 COMPONENTS SerialPort REQUIRED)

nx_add_server_plugin(ssc_analytics_plugin OPTIONAL
    PRIVATE_LIBS
        Qt5::Core
        Qt5::SerialPort
        nx_plugin_utils
        nx_vms_api
        nx_network
        nx_utils
        nx_fusion
        nx_kit
    ADD_TO_DISTRIBUTION_IF FALSE
)

add_definitions(-DNX_PRINT_TO_LOG)

# Project is under discussion
if(withDistributions AND customization STREQUAL "ionetworks" AND WINDOWS)
    add_subdirectory(distribution)
endif()
