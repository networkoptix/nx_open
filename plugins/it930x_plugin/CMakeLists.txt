nx_add_target(it930x_plugin LIBRARY
    ADDITIONAL_SOURCES
          "src/dtvapi_v14/DTVAPI.c"
          "src/dtvapi_v15/DTVAPI_15.c"
    NO_MOC NO_PCH PUBLIC_LIBS common)

target_compile_definitions(it930x_plugin PRIVATE DTVAPI_V15)
