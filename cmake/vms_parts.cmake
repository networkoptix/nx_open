set(_withMediaServer ON)
set(_withMediaDbUtil OFF)
set(_withTrayTool OFF)
set(_withNxTool OFF)
set(_withDesktopClient ON)
set(_withMobileClient ON)
set(_withClouds OFF)
set(_withTestCamera ON)
set(_withTests ON)
set(_withMiniLauncher OFF)
set(_withRootTool OFF)
set(_withP2PConnectionTestingUtility OFF)

if("${platform}" STREQUAL "linux")
    if("${arch}" MATCHES "arm|aarch64")
        set(_withDesktopClient OFF)
        set(_withMobileClient OFF)
        set(_withTests OFF)

        if("${box}" STREQUAL "bpi")
            set(_withTests ON)
        elseif("${box}" STREQUAL "tx1")
            set(_withDesktopClient ON)
            set(_withTests ON)
        endif()
    elseif("${arch}" STREQUAL "x86")
        set(_withTests OFF)
        set(_withClouds OFF)
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
    set(_withClouds ON)
endif()

if(WINDOWS)
    set(_withTrayTool ON)
    set(_withNxTool ${build_nxtool})
    if("${arch}" STREQUAL "x64")
        set(_withClouds ON)
    endif()
    if(NOT developerBuild)
        set(_withMiniLauncher ON)
    endif()
endif()

if(targetDevice STREQUAL "edge1")
    set(_withTestCamera OFF)
endif()

if(LINUX AND box MATCHES "none" AND NOT developerBuild)
    set(_withRootTool ON)
endif()

option(withMediaServer "Enable media server" ${_withMediaServer})
option(withMediaDbUtil "Enable media db util" ${_withMediaDbUtil})
option(withTrayTool "Enable tray tool" ${_withTrayTool})
option(withNxTool "Enable NxTool" ${_withNxTool})
option(withDesktopClient "Enable desktop client" ${_withDesktopClient})
option(withMobileClient "Enable mobile client" ${_withMobileClient})
option(withClouds "Enable cloud components" ${_withClouds})
option(withTestCamera "Enable test camera" ${_withTestCamera})
option(withTests "Enable unit tests" ${_withTests})
option(withCassandraTests "Enable cassandra related tests" ${_withCassandraTests})
option(withMiniLauncher "Enable minilauncher" ${_withMiniLauncher})
option(withScreenChecker "Enable screen checker" OFF)
option(withNovBrowser "Enable Nov Browser" OFF)
nx_option(withRootTool "Enable root tool" ${_withRootTool})
nx_option(
    withP2PConnectionTestingUtility
    "Enable P2P connection testing utility"
    ${_withP2PConnectionTestingUtility})

cmake_dependent_option(withDistributions "Enable distributions build"
    OFF "developerBuild"
    ON
)

cmake_dependent_option(withAnalyticsSdk "Enable nx_analytics_sdk build"
    OFF "NOT withDistributions"
    ON
)

cmake_dependent_option(withUnitTestsArchive "Enable unit tests archive generation"
    OFF "withTests"
    OFF
)

option(enableHanwha OFF "Enable hanwha camera vendor even if it is disabled by default")
mark_as_advanced(enableHanwha)

if(enableHanwha OR developerBuild AND NOT targetDevice STREQUAL "edge1")
    set(enable_hanwha true)
endif()

unset(_withMediaServer)
unset(_withMediaDbUtil)
unset(_withNxTool)
unset(_withTrayTool)
unset(_withDesktopClient)
unset(_withMobileClient)
unset(_withClouds)
unset(_withTestCamera)
unset(_withTests)
unset(_withCassandraTests)
unset(_withMiniLauncher)
unset(_withRootTool)
unset(_withP2PConnectionTestingUtility)
