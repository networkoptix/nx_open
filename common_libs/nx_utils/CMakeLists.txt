nx_configure_file(
    maven/filter-resources/app_info_impl.cpp
    ${CMAKE_CURRENT_BINARY_DIR})

nx_add_target(nx_utils LIBRARY
    ADDITIONAL_SOURCES
        "${CMAKE_CURRENT_BINARY_DIR}/app_info_impl.cpp"
    PUBLIC_LIBS
        boost Qt5::Core)

target_compile_definitions(nx_utils
    PRIVATE NX_UTILS_API=${API_EXPORT_MACRO}
    INTERFACE NX_UTILS_API=${API_IMPORT_MACRO})

if(MACOSX)
    find_library(Foundation_LIBRARY Foundation)
    mark_as_advanced(Foundation_LIBRARY)
    target_link_libraries(nx_utils PUBLIC ${Foundation_LIBRARY})
endif()
