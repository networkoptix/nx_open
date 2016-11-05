find_sources(src CPP_FILES)

configure_file(
    maven/filter-resources/nx_vms_utils_app_info_impl.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/nx_vms_utils_app_info_impl.cpp
)

add_library(nx_vms_utils
    ${CPP_FILES}
    ${CMAKE_CURRENT_BINARY_DIR}/nx_vms_utils_app_info_impl.cpp
    src/StdAfx.cpp src/StdAfx.h
)

add_precompiled_header(nx_vms_utils src/StdAfx.h FORCEINCLUDE)

target_include_directories(nx_vms_utils PUBLIC src)

target_compile_definitions(nx_vms_utils
    PRIVATE NX_VMS_UTILS_API=${API_EXPORT_MACRO}
    INTERFACE NX_VMS_UTILS_API=${API_IMPORT_MACRO})

target_link_libraries(nx_vms_utils PUBLIC nx_utils)
