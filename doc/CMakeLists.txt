nx_configure_file(
    Doxyfile.in
    ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)

file(COPY "index.html"
    DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")

if(doxygen_directory)
    add_custom_target(documentation
        COMMENT "Generating documentation..."
        COMMAND ${doxygen_directory}/bin/doxygen)
endif()
