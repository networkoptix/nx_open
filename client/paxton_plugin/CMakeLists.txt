project(paxton_plugin VERSION ${releaseVersion} LANGUAGES CSharp)

set(signKey "${CMAKE_CURRENT_SOURCE_DIR}/src/Sign.snk")

set(referenceList
    "System"
    "System.Core"
    "System.Windows.Forms"
    "log4net"
    "Paxton.Net2.OemDvrInterfaces"
)

add_library(paxton_plugin SHARED
    src/SampleMiniDriver.cs
    src/Properties/AssemblyInfo.cs
    ${signKey}
)

set_target_properties(paxton_plugin PROPERTIES
    VS_DOTNET_TARGET_FRAMEWORK_VERSION "v4.6.2"
    VS_DOTNET_REFERENCES "${referenceList}"
    VS_GLOBAL_SignAssembly "true"
    VS_GLOBAL_AssemblyOriginatorKeyFile "${signKey}"
    OUTPUT_NAME "${paxtonLibraryName}.OemDvrMiniDriver"
)
