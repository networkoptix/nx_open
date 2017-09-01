set(product.title "${minilauncher.name}")

nx_configure_file(maven/filter-resources/version.h ${CMAKE_CURRENT_BINARY_DIR})

nx_add_target(minilauncher EXECUTABLE
    PUBLIC_LIBS
        shlwapi.lib
)