## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

nx_add_target(nx_vms_utils LIBRARY
    PUBLIC_LIBS
        nx_utils
        nx_vms_api
    FOLDER common/libs
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
