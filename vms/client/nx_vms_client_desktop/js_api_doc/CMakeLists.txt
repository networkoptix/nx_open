## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

nx_configure_file(
    Doxyfile.in
    ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)

add_custom_target(js_api_documentation
    COMMENT "Generating JS API documentation..."
    COMMAND ${CONAN_DOXYGEN_ROOT}/bin/doxygen)

set(js_api_documentation_zip_file "${CMAKE_CURRENT_BINARY_DIR}/js_api_doc.zip")

add_custom_target(js_api_documentation_zip
    COMMENT "Packing documentation..."
    DEPENDS js_api_documentation
    BYPRODUCTS ${js_api_documentation_zip_file}
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/html"
    COMMAND ${CMAKE_COMMAND} -E tar cf ${js_api_documentation_zip_file} --format=zip ./)

set_target_properties(js_api_documentation js_api_documentation_zip
    PROPERTIES
        FOLDER js_api
        LOCATION ${js_api_documentation_zip_file})
