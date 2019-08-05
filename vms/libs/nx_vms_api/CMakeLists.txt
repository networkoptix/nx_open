nx_configure_file(
    protocol_version.cpp.in
    ${CMAKE_CURRENT_BINARY_DIR}/protocol_version.cpp)

set(library_type)
if(WINDOWS)
    set(library_type LIBRARY_TYPE SHARED)
endif()

nx_add_target(nx_vms_api LIBRARY ${library_type}
    ADDITIONAL_SOURCES
        "${CMAKE_CURRENT_BINARY_DIR}/protocol_version.cpp"
    PUBLIC_LIBS
        nx_utils nx_fusion)

target_compile_definitions(nx_vms_api
    # TODO: Rename NX_VMS_API to NX_VMS_API_API.
    PRIVATE NX_VMS_API=${API_EXPORT_MACRO}
    INTERFACE NX_VMS_API=${API_IMPORT_MACRO})

if(withTests)
    add_subdirectory(unit_tests)
endif()
