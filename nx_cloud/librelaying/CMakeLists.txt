set(product.title "${company.name} librelaying")
set(product.display.title "${company.name} Relaying library")

set(library_type)
if(WINDOWS)
    set(library_type LIBRARY_TYPE SHARED)
endif()

if(LINUX)
    add_compile_options("-Werror" "-Wall" "-Wextra")
endif()

nx_add_target(librelaying LIBRARY ${library_type}
    PUBLIC_LIBS
        nx_network nx_fusion nx_utils)

target_compile_definitions(librelaying
    PRIVATE NX_RELAYING_API=${API_EXPORT_MACRO}
    INTERFACE NX_RELAYING_API=${API_IMPORT_MACRO})

set_target_properties(librelaying PROPERTIES PREFIX "")
