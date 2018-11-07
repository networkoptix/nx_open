add_subdirectory(libs)
add_subdirectory(client)
add_subdirectory(server)

if(withTrayTool)
    add_subdirectory(traytool)
endif()
