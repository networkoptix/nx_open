set(library_type)
if(WINDOWS)
    set(library_type LIBRARY_TYPE SHARED)
endif()

nx_configure_file(
    maven/filter-resources/libvms_gateway_core_app_info_impl.cpp
    ${CMAKE_CURRENT_BINARY_DIR})

nx_add_target(vms_gateway_core LIBRARY ${library_type} NO_MOC
    ADDITIONAL_SOURCES
        "${CMAKE_CURRENT_BINARY_DIR}/libvms_gateway_core_app_info_impl.cpp"
    PUBLIC_LIBS
        common nx_network nx_fusion nx_utils
)

set(mocableNetworkFiles
    ${CMAKE_CURRENT_SOURCE_DIR}/src/nx/cloud/vms_gateway/vms_gateway_embeddable.h)

nx_add_qt_mocables(vms_gateway_core ${mocableNetworkFiles}
    INCLUDE_DIRS
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_compile_definitions(vms_gateway_core
    PRIVATE NX_VMS_GATEWAY_API=${API_EXPORT_MACRO}
    INTERFACE NX_VMS_GATEWAY_API=${API_IMPORT_MACRO})

if(withTests)
    add_subdirectory(unit_tests)
endif()
