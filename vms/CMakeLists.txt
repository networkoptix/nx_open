add_subdirectory(libs)
add_subdirectory(client)

if(withTrayTool)
    add_subdirectory(traytool)
endif()
