## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

find_package(prometheus-cpp REQUIRED)

nx_add_target(nx_prometheus LIBRARY
    NO_MOC
    PRIVATE_LIBS
        prometheus-cpp::core
    FOLDER common/libs
)

target_compile_definitions(nx_prometheus
    PRIVATE NX_PROMETHEUS_API=${API_EXPORT_MACRO}
    INTERFACE NX_PROMETHEUS_API=${API_IMPORT_MACRO})

if(withTests)
    add_subdirectory(unit_tests)
endif()
