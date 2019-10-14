execute_process(
    COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/gen_transaction_types.py
        --input=${CMAKE_CURRENT_SOURCE_DIR}/src/transaction/transaction.h
        --output=${CMAKE_CURRENT_BINARY_DIR}/transaction_types.h
)

nx_add_target(appserver2 LIBRARY 
    ADDITIONAL_SOURCES 
        "${CMAKE_CURRENT_BINARY_DIR}/transaction_types.h"
    PUBLIC_LIBS 
        common cloud_db_client nx_fusion nx_network
)

if(withTests AND (withMediaServer OR withClouds))
    add_subdirectory(unit_tests)
endif()
