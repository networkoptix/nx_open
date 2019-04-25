include_directories(include)

file(GLOB maxminddb_sources
    include/*.h
    src/*.h
    src/*.c)

add_library(maxminddb SHARED ${maxminddb_sources})

if (WIN32)
	target_link_libraries(maxminddb Ws2_32)
endif()

target_include_directories(maxminddb INTERFACE include)

#--------------------------------------------------------------------------------------------------
# mmdblookup executable (only works on Linux due to use of getopt.h)

if (UNIX)
    find_package(Threads)
    add_executable(mmdblookup bin/mmdblookup.c)
    target_link_libraries(mmdblookup maxminddb pthread)
    add_definitions(-DPACKAGE_VERSION=\"1.3.2\") # not sure why this is necessary
endif()