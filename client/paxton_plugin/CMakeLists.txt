project(paxton_plugin VERSION ${releaseVersion} LANGUAGES CSharp)

set(signKey "${CMAKE_CURRENT_SOURCE_DIR}/src/Sign.snk")

set(referenceList
    "System"
    "System.Core"
    "System.Windows.Forms"
    "${CMAKE_CURRENT_SOURCE_DIR}/References/log4net.dll"
    "${CMAKE_CURRENT_SOURCE_DIR}/References/Paxton.Net2.OemDvrInterfaces.dll"
    "${CMAKE_CURRENT_SOURCE_DIR}/References/Newtonsoft.Json.dll"
)

nx_configure_file(
    assembly_info.cs.in
    ${CMAKE_CURRENT_BINARY_DIR}/assembly_info.cs)

nx_configure_file(
    app_info.cs.in
    ${CMAKE_CURRENT_BINARY_DIR}/app_info.cs)

add_library(paxton_plugin SHARED
    src/drv_mini_driver_implementation.cs
    src/media_server_api.cs
    ${CMAKE_CURRENT_BINARY_DIR}/assembly_info.cs
    ${CMAKE_CURRENT_BINARY_DIR}/app_info.cs
    ${signKey}
)

set_target_properties(paxton_plugin PROPERTIES
    VS_DOTNET_TARGET_FRAMEWORK_VERSION "v4.6.2"
    VS_DOTNET_REFERENCES "${referenceList}"
    VS_GLOBAL_SignAssembly "true"
    VS_GLOBAL_AssemblyOriginatorKeyFile "${signKey}"
    OUTPUT_NAME "${paxtonLibraryName}.OemDvrMiniDriver"
)
