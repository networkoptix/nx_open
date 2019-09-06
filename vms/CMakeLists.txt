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

if(withVmsTools)
    add_subdirectory(tools)
endif()

set(_boxToolSupported OFF)
if("${platform}" STREQUAL "linux")
    if("${arch}" STREQUAL "x64")
        set(_boxToolSupported ON)
    endif()
endif()
if(WINDOWS)
    set(_boxToolSupported ON)
endif()

if(withBoxTool AND withDistributions AND _boxToolSupported)
    add_subdirectory(nx_box_tool)
endif()

unset(_boxToolSupported)
