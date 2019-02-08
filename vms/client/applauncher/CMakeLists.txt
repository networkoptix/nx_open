nx_configure_file(maven/filter-resources/applauncher_app_info_impl.cpp
    ${CMAKE_CURRENT_BINARY_DIR})

nx_add_target(applauncher EXECUTABLE SIGNED
    ADDITIONAL_SOURCES
        "${CMAKE_CURRENT_BINARY_DIR}/applauncher_app_info_impl.cpp"
    RC_FILE
        "applauncher.rc"
    PUBLIC_LIBS
        qtsinglecoreapplication
        common
        nx_vms_applauncher_api
        nx_kit
)

string(REPLACE ".exe" "" binary_name ${applauncher.binary.name})
set_target_properties(applauncher PROPERTIES OUTPUT_NAME ${binary_name})
