## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

include(utils)

set(PLUGIN_NAME mqtt_plugin)

set(PYTHON_VERSION_MAJOR 3)
set(PYTHON_VERSION_MINOR 12)

if(WIN32)
    set(PYTHON_LIBRARY "${CONAN_CPYTHON_ROOT}/bin/libs/python312.lib")
    set(PYTHON_EXECUTABLE_DIR "${CONAN_CPYTHON_ROOT}/bin")
    set(PYTHON_EXECUTABLE "${PYTHON_EXECUTABLE_DIR}/python.exe")
    set(PYTHON_INCLUDE_DIR "${CONAN_CPYTHON_ROOT}/bin/include")
else()
    set(PYTHON_LIBRARY "${CONAN_CPYTHON_ROOT}/lib/libpython3.12.a")
    set(PYTHON_EXECUTABLE "${CONAN_CPYTHON_ROOT}/bin/python")
    set(PYTHON_INCLUDE_DIR "${CONAN_CPYTHON_ROOT}/include/python3.12")
endif()

#--------------------------------------------------------------------------------------------------
# Prepare Python re-distribution directory for the plugin distribution.

# Copy Python runtime files to the re-distribution directory.
set(PYTHON_REDIST_DIR ${CMAKE_BINARY_DIR}/mqtt_plugin_python_files)
if(WIN32)
    set(lib_file_path
        ${CONAN_CPYTHON_ROOT}/bin/python${PYTHON_VERSION_MAJOR}${PYTHON_VERSION_MINOR}.dll)
    nx_copy(${CONAN_CPYTHON_ROOT}/bin/Lib DESTINATION ${PYTHON_REDIST_DIR} IF_NEWER)
    nx_copy(${CONAN_CPYTHON_ROOT}/bin/DLLs DESTINATION ${PYTHON_REDIST_DIR} IF_NEWER)
    nx_copy(${lib_file_path} DESTINATION ${PYTHON_REDIST_DIR} IF_NEWER)
    set(PYTHON_REDIST_LIB_PATH ${PYTHON_REDIST_DIR}/Lib)
else()
    set(python_lib_subdir "lib/python${PYTHON_VERSION_MAJOR}.${PYTHON_VERSION_MINOR}")
    nx_copy(
        ${CONAN_CPYTHON_ROOT}/${python_lib_subdir} DESTINATION ${PYTHON_REDIST_DIR}/lib IF_NEWER
    )
    set(PYTHON_REDIST_LIB_PATH ${PYTHON_REDIST_DIR}/${python_lib_subdir})
    if(${CONAN_CPYTHON_ROOT}/${python_lib_subdir} IS_NEWER_THAN ${PYTHON_REDIST_LIB_PATH})
        execute_process(
            COMMAND find -name *.so -exec ${CMAKE_STRIP} -S {} \\;
            WORKING_DIRECTORY ${PYTHON_REDIST_LIB_PATH}
        )
    endif()
endif()

# Remove directories not necessary for the plugin.
set(unused_dirs
    __pycache__
    config-${PYTHON_VERSION_MAJOR}.${PYTHON_VERSION_MINOR}-x86_64-linux-gnu
    ensurepip
    idlelib
    lib2to3
    pydoc_data
    site-packages
    tkinter
    turtledemo
    unittest
)
foreach(dir IN ITEMS ${unused_dirs})
    execute_process(COMMAND ${CMAKE_COMMAND} -E rm -rf ${PYTHON_REDIST_LIB_PATH}/${dir})
endforeach()

# Install 3rd-paty Python packages required by the plugin to the re-distribution directory.
set(REQUIREMENTS_FILE ${CMAKE_CURRENT_SOURCE_DIR}/python_src/requirements.txt)
set(PYTHON_PACKAGE_DIRECTORY ${PYTHON_REDIST_LIB_PATH}/site-packages)
if (${REQUIREMENTS_FILE} IS_NEWER_THAN ${PYTHON_PACKAGE_DIRECTORY})
    if(WIN32)
        # Python cannot find the OpenSSL DLLs, so we copy them to the Python directort. #TODO: Try
        # to find more efficient way to solve this issue.
        execute_process(
            COMMAND ${CMAKE_COMMAND} -E copy -t "${PYTHON_EXECUTABLE_DIR}"
                "${CONAN_OPENSSL_ROOT}/bin/libcrypto-1_1-x64.dll"
                "${CONAN_OPENSSL_ROOT}/bin/libssl-1_1-x64.dll"
        )
    endif()

    execute_process(
        COMMAND
            ${CMAKE_COMMAND} -E env LD_LIBRARY_PATH=${CONAN_OPENSSL_ROOT}/lib ${PYTHON_EXECUTABLE}
                -m pip install -r ${REQUIREMENTS_FILE} --target ${PYTHON_PACKAGE_DIRECTORY}
    )

    if(WIN32)
        execute_process(
            COMMAND ${CMAKE_COMMAND} -E rm
                "${PYTHON_EXECUTABLE_DIR}/libcrypto-1_1-x64.dll"
                "${PYTHON_EXECUTABLE_DIR}/libssl-1_1-x64.dll"
        )
    else()
        execute_process(
            COMMAND find ${PYTHON_PACKAGE_DIRECTORY} -name *.so -exec ${CMAKE_STRIP} -S {} "\\\;"
        )
    endif()
endif()

nx_store_known_files_in_directory(${PYTHON_REDIST_DIR})

file(GLOB PYTHON_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/python_src/*)
list(FILTER PYTHON_SOURCES EXCLUDE REGEX "(_nx_submodule|requirements.txt|debug)$")
nx_copy(${PYTHON_SOURCES} DESTINATION ${PYTHON_REDIST_DIR} IF_DIFFERENT)

# -------------------------------------------------------------------------------------------------
# Build the plugin.

file(GLOB PYTHON_REDIST_FILES ${PYTHON_REDIST_DIR}/*)
nx_add_server_plugin(${PLUGIN_NAME} NO_MOC DEDICATED_DIR OPTIONAL
    PRIVATE_LIBS
        nx_sdk
        nx_kit
        ${PYTHON_LIBRARY}
    EXTRA_FILES
        ${PYTHON_REDIST_FILES}
)

target_include_directories(${PLUGIN_NAME} PRIVATE
    ${open_source_root}/artifacts/pybind11/include
    ${PYTHON_INCLUDE_DIR}
)

if(LINUX)
    target_link_libraries(${PLUGIN_NAME} PRIVATE pthread dl util)
endif()

# Strip the plugin library in Release configuration.
add_custom_command(
    TARGET ${PLUGIN_NAME} POST_BUILD
    DEPENDS ${PLUGIN_NAME}
    COMMAND $<$<CONFIG:Release>:${CMAKE_STRIP}>
    ARGS --strip-all $<TARGET_FILE:${PLUGIN_NAME}>
)
