cmake_minimum_required(VERSION 3.10.0 FATAL_ERROR)

set(releaseVersion 4.0.0)

project(paxton_plugin VERSION ${releaseVersion} LANGUAGES CSharp)

set(customization "default" CACHE STRING "Product customization")
set(dotNetFramework "v4.6.2" CACHE STRING "Target .NET Framework version. Allowed: v4.0, v4.6.2")
set(CMAKE_BUILD_TYPE "Release" CACHE STRING "One of [Debug Release RelWithDebInfo MinSizeRel].")
set(buildNumber "0" CACHE STRING "Build number")
option(codeSigning "Enable code signing" OFF)

set(signKey "${CMAKE_CURRENT_SOURCE_DIR}/src/Sign.snk")
set(repositoryRootDir ${PROJECT_SOURCE_DIR}/..)
set(releaseVersion.full ${releaseVersion}.${buildNumber})

if("${dotNetFramework}" STREQUAL "v4.0")
    set(net2Version "Net2v5")
elseif("${dotNetFramework}" STREQUAL "v4.6.2")
    set(net2Version "Net2v6")
else()
    message(FATAL_ERROR "Unknown .Net framework version ${dotNetFramework}")
endif()

list(APPEND CMAKE_MODULE_PATH
    ${repositoryRootDir}/cmake
)

set(build_utils_dir "${repositoryRootDir}/build_utils" CACHE PATH "Path to build utils")
set(rdep_dir "${build_utils_dir}/python" CACHE PATH "Path to rdep scripts")
include(find_python)
include(rdep)

set(customization_dir "${repositoryRootDir}/customization/${customization}")
include(utils)
include(${customization_dir}/../default-values.cmake)
include(${customization_dir}/customization.cmake)

nx_rdep_configure()

set(ENV{PYTHONPATH} ${rdep_dir})
set(depenencies_cmake_file ${CMAKE_BINARY_DIR}/dependencies.cmake)
execute_process(
    COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_SOURCE_DIR}/sync_dependencies.py
        --packages-dir=${PACKAGES_DIR}
        --cmake-include-file=${depenencies_cmake_file}
        --customization=${customization}
        --dot-net-framework=${dotNetFramework}
    RESULT_VARIABLE sync_result
)

include(cloud_host)
include(output_directories)
include(distribution_names)
include(option)
include(windows_signing)

set(minilauncher.binary.name "${product.name} Launcher.exe") #< Cannot include properties.cmake.
set(paxtonDriverOutputName "${paxtonLibraryName}.OemDvrMiniDriver")
set(paxtonDriverIntermediateName "${paxtonLibraryName}.OemDvrMiniDriver.mini")

if(NOT sync_result STREQUAL "0")
    message(FATAL_ERROR "error: Packages sync failed")
endif()

include(${depenencies_cmake_file})

set(referenceList
    "System"
    "System.Core"
    "System.Windows.Forms"
    "System.Net"
    "${references_directory}/bin/log4net.dll"
    "${references_directory}/bin/Paxton.Net2.OemDvrInterfaces.dll"
    "${references_directory}/bin/Newtonsoft.Json.dll"
)

set(CMAKE_CSharp_FLAGS /platform:x86)

nx_configure_file(
    assembly_info.cs.in
    ${CMAKE_CURRENT_BINARY_DIR}/assembly_info.cs)

nx_configure_file(
    app_info.cs.in
    ${CMAKE_CURRENT_BINARY_DIR}/app_info.cs)

nx_configure_file(
    log4net.config
    ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
    COPYONLY)

add_library(paxton_plugin SHARED
    src/dvr_mini_driver.cs
    src/paxton_client.cs
    src/media_server_api.cs
    src/desktop_client_api.cs
    ${CMAKE_CURRENT_BINARY_DIR}/assembly_info.cs
    ${CMAKE_CURRENT_BINARY_DIR}/app_info.cs
    ${signKey}
)

set_target_properties(paxton_plugin PROPERTIES
    VS_DOTNET_TARGET_FRAMEWORK_VERSION ${dotNetFramework}
    VS_DOTNET_REFERENCES "${referenceList}"
    VS_GLOBAL_SignAssembly "true"
    VS_GLOBAL_AssemblyOriginatorKeyFile "${signKey}"
    OUTPUT_NAME "${paxtonDriverIntermediateName}"
)

# Merging references into a single library. Note: OemDvrInterfaces.dll is not to be merged!
add_custom_command(TARGET paxton_plugin POST_BUILD
    COMMAND
        ${ilmerge_directory}/bin/ilmerge.exe
        /out:"${paxtonDriverOutputName}.dll"
        "${paxtonDriverIntermediateName}.dll"
        "${references_directory}/bin/Newtonsoft.Json.dll"
        "${references_directory}/bin/log4net.dll"
        /target:dll
        /targetplatform:v4,C:/Windows/Microsoft.NET/Framework64/v4.0.30319
        /wildcards
    COMMENT "Merging references...")

target_compile_options(paxton_plugin PRIVATE "/langversion:6")

# Test binary to check dll functionality without Paxton itself.
add_executable(paxton_plugin_test
    src/test.cs)
target_link_libraries(paxton_plugin_test paxton_plugin)

set_target_properties(paxton_plugin_test PROPERTIES
    VS_DOTNET_TARGET_FRAMEWORK_VERSION ${dotNetFramework}
    VS_DOTNET_REFERENCES "${referenceList}"
    OUTPUT_NAME "${paxtonDriverOutputName}-test"
)

target_compile_options(paxton_plugin_test PRIVATE "/langversion:6")

add_subdirectory(distribution)
