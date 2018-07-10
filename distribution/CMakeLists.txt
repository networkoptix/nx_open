if(WINDOWS)
    add_subdirectory(wix)
elseif(LINUX AND box STREQUAL "none") #< Desktop Linux.
    add_subdirectory(deb)
elseif(MACOSX)
    add_subdirectory(dmg)
elseif(targetDevice MATCHES "bpi|bananapi|rpi|edge1")
    add_subdirectory(tar)
endif()
