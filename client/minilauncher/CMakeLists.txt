set(product.title "${minilauncher.name}")

nx_configure_file(maven/filter-resources/version.h ${CMAKE_CURRENT_BINARY_DIR})

nx_add_target(minilauncher EXECUTABLE
    PUBLIC_LIBS
        shlwapi.lib
)
if(WINDOWS AND CMAKE_BUILD_TYPE STREQUAL "Release")
    add_custom_command(
        TARGET minilauncher
        COMMAND ${CMAKE_COMMAND}
        ARGS -E copy "${bin_source_dir}/minilauncher.exe" "${bin_source_dir}/${minilauncher.binary.name}"
    )
endif()