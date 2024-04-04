## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

nx_add_target(nx_vms_rules
    LIBRARY
    PUBLIC_LIBS
        nx_vms_common
    FOLDER common/libs
)

nx_make_target_translatable(nx_vms_rules COMPONENTS "client" "server")

target_compile_definitions(nx_vms_rules
    PRIVATE NX_VMS_RULES_API=${API_EXPORT_MACRO}
    INTERFACE NX_VMS_RULES_API=${API_IMPORT_MACRO})

if(withTests)
    add_subdirectory(unit_tests)
endif()
