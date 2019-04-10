nx_find_files(c_files src "c")
nx_add_target(nx_onvif LIBRARY LIBRARY_TYPE STATIC NO_MOC
    ADDITIONAL_SOURCES ${c_files}
    PUBLIC_LIBS openssl
)

set_source_files_properties(${c_files} PROPERTIES LANGUAGE CXX)

target_compile_definitions(nx_onvif
    PUBLIC 
        WITH_OPENSSL=
        WITH_PURE_VIRTUAL=
        WITH_C_LOCALE=
)

target_include_directories(nx_onvif 
    PUBLIC 
    	${CMAKE_CURRENT_SOURCE_DIR}/src/onvif
        ${CMAKE_CURRENT_SOURCE_DIR}/src/gsoap
)    

if(NOT WINDOWS)
    target_link_libraries(nx_onvif PUBLIC pthread)
endif()

# This generated file provides tons of warning messages, so disable them.
set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/src/onvif/soapC.cpp
    PROPERTIES COMPILE_FLAGS "-w")
