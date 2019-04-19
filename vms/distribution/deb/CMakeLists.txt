# Variables used by build_distribution.conf files and/or .deb contents.
if(arch STREQUAL "x64")
    set(os.arch "amd64")
elseif(arch STREQUAL "x86")
    set(os.arch "i386")
elseif(arch STREQUAL "arm64")
    set(os.arch "arm64")
elseif(arch STREQUAL "arm32")
    set(os.arch "armhf")
endif()
set(target.arch "${os.arch}")

message(STATUS "Generating license")
execute_process(
    COMMAND ${pandoc_directory}/bin/pandoc
        -s ${customization.dir}/license.html
        -t plain --wrap none -o ${CMAKE_CURRENT_BINARY_DIR}/license.txt
    RESULT_VARIABLE pandoc_result
)

if(NOT pandoc_result EQUAL 0)
    message(FATAL_ERROR "error: License generation failed (${pandoc_result})")
endif()

# Read license.txt and properly format it to be included into deb templates file:
#  - All lines but the first one must start with a space.
#  - Empty lines must contain a single period (preceded by a space too).
file(READ ${CMAKE_CURRENT_BINARY_DIR}/license.txt license.text)
string(REGEX REPLACE "\n\n" "\n.\n" license.text "${license.text}")
string(REGEX REPLACE "\n" "\n " license.text "${license.text}")

nx_configure_file(
    ${CMAKE_SOURCE_DIR}/build_environment/maven/filter-resources/version.py
    ${CMAKE_BINARY_DIR}/version.py
)

if(withMediaServer)
    add_subdirectory(mediaserver)
endif()

if(withDesktopClient)
    add_subdirectory(client)
endif()
