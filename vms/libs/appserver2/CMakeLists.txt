## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

set(transaction_types_file ${CMAKE_CURRENT_BINARY_DIR}/transaction_types.h)
add_custom_command(
    OUTPUT ${transaction_types_file}
    COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/gen_transaction_types.py
        --input=${CMAKE_CURRENT_SOURCE_DIR}/src/transaction/transaction.h
        --output=${transaction_types_file}
    DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/src/transaction/transaction.h
)

nx_add_target(appserver2 LIBRARY NO_API_MACROS
    ADDITIONAL_SOURCES
        ${transaction_types_file}
    PUBLIC_LIBS
        nx_vms_common
        nx_vms_license
        cloud_db_client
        nx_fusion
        nx_network
    FOLDER common/libs
)
target_include_directories(appserver2 PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

if(withTests AND (withMediaServer OR withClouds OR openSourceBuild))
    add_subdirectory(unit_tests)
endif()
