find_sources(src CPP_FILES)

add_library(cloud_db_client
    ${CPP_FILES}
    src/StdAfx.cpp src/StdAfx.h
)

add_precompiled_header(cloud_db_client src/StdAfx.h FORCEINCLUDE)

target_include_directories(cloud_db_client PUBLIC src src/include)

target_link_libraries(cloud_db_client PUBLIC common)
