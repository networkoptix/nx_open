set(_withMediaServer ON)
set(_withTrayTool OFF)
set(_withNxTool OFF)
set(_withDesktopClient ON)
set(_withMobileClient ON)
set(_withClouds OFF)
set(_withTestCamera ON)
set(_withTests ON)

if("${platform}" STREQUAL "linux")
    if("${arch}" MATCHES "arm|aarch64")
        set(_withDesktopClient OFF)
        set(_withMobileClient OFF)
        set(_withTests OFF)

        if("${box}" STREQUAL "bpi")
            set(_withMobileClient ON)
            set(_withTests ON)
        elseif("${box}" STREQUAL "tx1")
            set(_withDesktopClient ON)
            set(_withTests ON)
        endif()
    elseif("${arch}" STREQUAL "x86")
        set(_withTests OFF)
    elseif("${arch}" STREQUAL "x64")
        set(_withClouds ON)
    endif()
endif()

if("${platform}" MATCHES "android|ios")
    set(_withMediaServer OFF)
    set(_withDesktopClient OFF)
    set(_withTests OFF)
    set(_withTestCamera OFF)
endif()

if("${platform}" STREQUAL "macosx")
    set(_withMediaServer OFF)
endif()

if(WINDOWS)
    set(_withTrayTool ON)
    set(_withNxTool ON)
    if("${arch}" STREQUAL "x64")
        set(_withClouds ON)
    endif()
endif()

if("${box}" STREQUAL "edge1")
    set(_withTestCamera OFF)
endif()

option(withMediaServer "Enable media server" ${_withMediaServer})
option(withTrayTool "Enable tray tool" ${_withTrayTool})
option(withNxTool "Enable NxTool" ${_withNxTool})
option(withDesktopClient "Enable desktop client" ${_withDesktopClient})
option(withMobileClient "Enable mobile client" ${_withMobileClient})
option(withClouds "Enable cloud components" ${_withClouds})
option(withTestCamera "Enable test camera" ${_withTestCamera})
option(withTests "Enable unit tests" ${_withTests})
option(withPluginStubs "Enable plugin stubs" ON)

cmake_dependent_option(withDistributions "Enable distributions build"
    OFF "developerBuild"
    ON
)

unset(_withMediaServer)
unset(_withNxTool)
unset(_withTrayTool)
unset(_withDesktopClient)
unset(_withMobileClient)
unset(_withClouds)
unset(_withTestCamera)
unset(_withTests)
