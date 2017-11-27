nx_add_target(it930x_plugin LIBRARY
    ADDITIONAL_SOURCES
          "${PROJECT_SOURCE_DIR}/plugins/it930x_plugin/src/dtvapi_v15/DTVAPI_15.c"
    NO_MOC NO_PCH PUBLIC_LIBS common)

target_compile_definitions(it930x_plugin PRIVATE DTVAPI_V15)
