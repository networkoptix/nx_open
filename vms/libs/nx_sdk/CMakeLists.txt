# nx_sdk is a static lib because it needs a dedicated instance of its context (global variables) in
# each Plugin and in the Server. Also, only one dynamic library of the Server can link with nx_sdk.
nx_add_target(nx_sdk LIBRARY LIBRARY_TYPE STATIC NO_MOC NO_PCH
    PUBLIC_LIBS
        nx_kit
)

target_compile_definitions(nx_sdk
    PRIVATE NX_SDK_API=${API_EXPORT_MACRO} #< Used to export nxPluginContext().
    INTERFACE NX_SDK_API= #< No need to import nxPluginContext() - called only by name in runtime.
)

if(withTests)
    add_subdirectory(unit_tests)
endif()
