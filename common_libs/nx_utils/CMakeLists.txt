nx_configure_file(
    maven/filter-resources/app_info_impl.cpp
    ${CMAKE_CURRENT_BINARY_DIR})

set(library_type)
if(WINDOWS)
    set(library_type LIBRARY_TYPE SHARED)
endif()

if(LINUX)
    add_compile_options("-Werror" "-Wall" "-Wextra")
endif()

if(IOS)
    set(ios_sources "src/nx/utils/log/log_ios.mm")
endif()

nx_add_target(nx_utils LIBRARY ${library_type}
    ADDITIONAL_SOURCES
        "${CMAKE_CURRENT_BINARY_DIR}/app_info_impl.cpp"
        ${ios_sources}
    PUBLIC_LIBS
        boost openssl Qt5::Core Qt5::Xml Qt5::Sql
        nx_kit
)

target_compile_definitions(nx_utils
    PRIVATE NX_UTILS_API=${API_EXPORT_MACRO}
    INTERFACE NX_UTILS_API=${API_IMPORT_MACRO})

if(WINDOWS)
    target_link_libraries(nx_utils PRIVATE DbgHelp ws2_32)
endif()
    
if(LINUX AND NOT ANDROID)
    target_link_libraries(nx_utils PRIVATE pthread)
endif()
    
if(MACOSX)
    find_library(Foundation_LIBRARY Foundation)
    mark_as_advanced(Foundation_LIBRARY)
    target_link_libraries(nx_utils PUBLIC ${Foundation_LIBRARY} z)
endif()
