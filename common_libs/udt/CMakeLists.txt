find_sources(src CPP_FILES HPP_FILES)

add_library(udt
    ${CPP_FILES}
    src/StdAfx.cpp src/StdAfx.h
)

add_precompiled_header(udt src/StdAfx.h FORCEINCLUDE)

target_include_directories(udt PUBLIC src)

target_compile_definitions(udt
    PRIVATE UDT_API=${API_EXPORT_MACRO}
    INTERFACE UDT_API=${API_IMPORT_MACRO}
)
