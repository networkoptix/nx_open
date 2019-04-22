nx_configure_file(
    app_info.cpp.in
    ${CMAKE_CURRENT_BINARY_DIR}/app_info.cpp)

set(library_type)
if(WINDOWS)
    set(library_type LIBRARY_TYPE SHARED)
endif()

nx_add_target(nx_vms_utils LIBRARY ${library_type}
    ADDITIONAL_SOURCES
        "${CMAKE_CURRENT_BINARY_DIR}/app_info.cpp"
    PUBLIC_LIBS
        nx_utils
        nx_vms_api
)

target_compile_definitions(nx_vms_utils
    PRIVATE NX_VMS_UTILS_API=${API_EXPORT_MACRO}
    INTERFACE NX_VMS_UTILS_API=${API_IMPORT_MACRO})

if(MACOSX)
    find_library(CoreServices_LIBRARY CoreServices)
    mark_as_advanced(CoreServices_LIBRARY)
    target_link_libraries(nx_vms_utils PUBLIC ${CoreServices_LIBRARY})
endif()

if(withTests)
    add_subdirectory(unit_tests)
endif()
