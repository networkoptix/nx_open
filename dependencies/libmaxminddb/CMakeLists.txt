set(libmaxminddb_source_dir ${CMAKE_SOURCE_DIR}/artifacts/libmaxminddb)

include_directories(${libmaxminddb_source_dir}/include)

file(GLOB libmaxminddb_sources
    ${libmaxminddb_source_dir}/include/*.h
    ${libmaxminddb_source_dir}/src/*.h
    ${libmaxminddb_source_dir}/src/*.c)

# The library parses doubles differently depending on endianness, using its own macro
# MMDB_LITTLE_ENDIAN
include(TestBigEndian)
TEST_BIG_ENDIAN(is_big_endian)
if (NOT is_big_endian)
    add_definitions(-DMMDB_LITTLE_ENDIAN)
endif()

set(library_type SHARED)
if (WINDOWS)
# STATIC on windows because source code does not export symbols, causing failed linkage
	set(library_type STATIC)
endif()

add_library(maxminddb ${library_type} ${libmaxminddb_sources})

if (WIN32)
	target_link_libraries(maxminddb Ws2_32)
endif()

target_include_directories(maxminddb INTERFACE ${libmaxminddb_source_dir}/include)

#--------------------------------------------------------------------------------------------------
# mmdblookup executable (only works on Linux due to use of getopt.h)

if (UNIX)
    find_package(Threads)
    add_definitions(-DVERSION="\\"1.3.2\\"")
    add_definitions(-DPACKAGE_VERSION="\\"1.3.2\\"")
    add_executable(mmdblookup ${libmaxminddb_source_dir}/bin/mmdblookup.c)
    target_link_libraries(mmdblookup maxminddb Threads::Threads)
endif()