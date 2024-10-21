## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

nx_add_test(nx_vms_rules_ut
    PUBLIC_LIBS
        nx_vms_common_test_support
        nx_vms_rules
    PROJECT VMS
    COMPONENT Client
    SUITES
    FOLDER common/tests
)

set(mocableFiles
    ${CMAKE_CURRENT_SOURCE_DIR}/src/nx/vms/rules/mock_action_builder_events.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/nx/vms/rules/mock_engine_events.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/nx/vms/rules/test_action.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/nx/vms/rules/test_event.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/nx/vms/rules/test_field.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/nx/vms/rules/action_field_validators/text_with_fields_validator_ut.cpp)

nx_add_qt_mocables(nx_vms_rules_ut ${mocableFiles}
    INCLUDE_DIRS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/nx/vms/rules")
