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