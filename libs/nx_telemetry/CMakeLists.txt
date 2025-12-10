## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

find_package(opentelemetry-cpp REQUIRED)

nx_add_target(nx_telemetry LIBRARY
    NO_MOC
    WERROR_IF LINUX
    PUBLIC_LIBS
        nx_utils
    PRIVATE_LIBS
        opentelemetry-cpp::opentelemetry-cpp
    FOLDER cloud/libs
)

target_compile_definitions(nx_telemetry
    PRIVATE NX_TELEMETRY_API=${API_EXPORT_MACRO}
    INTERFACE NX_TELEMETRY_API=${API_IMPORT_MACRO})
