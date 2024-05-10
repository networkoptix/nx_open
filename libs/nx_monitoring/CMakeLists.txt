## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

nx_add_target(nx_monitoring LIBRARY WERROR NO_MOC
    PUBLIC_LIBS
        Qt6::Core
        Qt6::Network
        nx_utils
    FOLDER common/libs
)

target_compile_definitions(nx_monitoring
    PRIVATE NX_MONITORING_API=${API_EXPORT_MACRO}
    INTERFACE NX_MONITORING_API=${API_IMPORT_MACRO})

if(MACOSX)
    find_library(CoreFoundation_LIBRARY CoreFoundation)
    mark_as_advanced(CoreFoundation_LIBRARY)

    find_library(IOKit_LIBRARY IOKit)
    mark_as_advanced(IOKit_LIBRARY)

    target_link_libraries(nx_monitoring
        PUBLIC
            ${CoreFoundation_LIBRARY}
            ${IOKit_LIBRARY})

elseif(WIN32)
    target_link_libraries(nx_monitoring
        PUBLIC
            iphlpapi.lib
            pdh.lib)
endif()
