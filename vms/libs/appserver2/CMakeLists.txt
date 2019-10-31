set(transaction_types_file ${CMAKE_CURRENT_BINARY_DIR}/transaction_types.h)
add_custom_command(
    OUTPUT ${transaction_types_file}
    COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/gen_transaction_types.py
        --input=${CMAKE_CURRENT_SOURCE_DIR}/src/transaction/transaction.h
        --output=${transaction_types_file}
    DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/src/transaction/transaction.h
)

nx_add_target(appserver2 LIBRARY
    ADDITIONAL_SOURCES
        ${transaction_types_file}
    PUBLIC_LIBS
        common cloud_db_client nx_fusion nx_network
)

if(withTests AND (withMediaServer OR withClouds))
    add_subdirectory(unit_tests)
endif()
