cmake_minimum_required(VERSION 2.6)

include_directories(include)

file(GLOB maxminddb_sources
    include/*.h
    src/*.h
    src/*.c)

# The library parses doubles differently depending on endianness, using its own macro
# MMDB_LITTLE_ENDIAN
include(TestBigEndian)
TEST_BIG_ENDIAN(is_big_endian)
if (NOT is_big_endian)
    add_definitions(-DMMDB_LITTLE_ENDIAN)
endif()

add_library(maxminddb SHARED ${maxminddb_sources})

if (WIN32)
	target_link_libraries(maxminddb Ws2_32)
endif()

target_include_directories(maxminddb INTERFACE include)

#--------------------------------------------------------------------------------------------------
# mmdblookup executable (only works on Linux due to use of getopt.h)

if (UNIX)
    find_package(Threads)
    add_definitions(-DVERSION="\\"1.3.2\\"")
    add_definitions(-DPACKAGE_VERSION="\\"1.3.2\\"")
    add_executable(mmdblookup bin/mmdblookup.c)
    target_link_libraries(mmdblookup maxminddb Threads::Threads)
endif()