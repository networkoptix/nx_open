set(product.title "${minilauncher.name}")

nx_configure_file(maven/filter-resources/version.h ${CMAKE_CURRENT_BINARY_DIR})

nx_add_target(minilauncher EXECUTABLE SIGNED
    RC_FILE
        "minilauncher.rc"
    PUBLIC_LIBS
        shlwapi.lib
)

string(REPLACE ".exe" "" binary_name ${minilauncher.binary.name})
set_target_properties(minilauncher PROPERTIES OUTPUT_NAME ${binary_name})
