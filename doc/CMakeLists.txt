nx_configure_file(
    Doxyfile.in
    ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)

file(COPY "index.html"
    DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")

if(doxygen_directory)
    add_custom_target(documentation
        COMMENT "Generating documentation..."
        COMMAND ${doxygen_directory}/bin/doxygen
    )
endif()

if(TARGET documentation)
    set(documentation_zip_file "${distribution_output_dir}/doc.zip")

    add_custom_target(documentation_zip
        COMMENT "Packing documentation..."
        DEPENDS documentation
        COMMAND ${CMAKE_COMMAND} -E tar cfv ${documentation_zip_file} --format=zip
            "${CMAKE_CURRENT_BINARY_DIR}/index.html"
            "${CMAKE_CURRENT_BINARY_DIR}/html"
    )

endif()
