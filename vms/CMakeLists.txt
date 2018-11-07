add_subdirectory(libs)
add_subdirectory(client)
add_subdirectory(server)

if(withTrayTool)
    add_subdirectory(traytool)
endif()

if(withTestCamera)
    add_subdirectory(testcamera)
endif()

if(withNxTool)
    add_subdirectory(nxtool)
endif()

if(withDistributions)
    add_subdirectory(distribution)
endif()
