set(library_type)
if(WINDOWS)
    set(library_type LIBRARY_TYPE SHARED)
endif()

set(product.title "${company.name} nx_data_sync_engine")
set(product.display.title "${company.name} P2p Data Sync Engine")

nx_add_target(nx_data_sync_engine LIBRARY ${library_type} NO_MOC
    WERROR_IF NOT WINDOWS
    PUBLIC_LIBS appserver2 nx_network)

target_compile_definitions(nx_data_sync_engine
    PRIVATE NX_DATA_SYNC_ENGINE_API=${API_EXPORT_MACRO}
    INTERFACE NX_DATA_SYNC_ENGINE_API=${API_IMPORT_MACRO})
