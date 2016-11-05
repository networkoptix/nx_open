find_sources(src CPP_FILES)

add_library(nx_email
    ${CPP_FILES}
    src/StdAfx.cpp src/StdAfx.h
)

add_precompiled_header(nx_email src/StdAfx.h FORCEINCLUDE)

target_include_directories(nx_email PUBLIC src)
target_link_libraries(nx_email PUBLIC common)
