if(NOT rdep_dir)
    set(rdep_dir "${CMAKE_SOURCE_DIR}/build_utils/python")
endif()

set(packagesDir "" CACHE STRING "Custom RDep repository directory")
mark_as_advanced(packagesDir)

if(packagesDir)
    set(PACKAGES_DIR ${packagesDir})
else()
    set(PACKAGES_DIR $ENV{RDEP_PACKAGES_DIR})
    if(NOT PACKAGES_DIR)
        # TODO: This block is for backward compatibility. Remove it when CI is re-configured.
        set(PACKAGES_DIR $ENV{environment})
        if(PACKAGES_DIR)
            string(APPEND PACKAGES_DIR /packages)
        endif()
    endif()
endif()

if(NOT PACKAGES_DIR)
    set(message "RDep packages dir is not specified. Please provide one of:")
    string(APPEND message "\n  RDEP_PACKAGES_DIR environment variable")
    string(APPEND message "\n  packagesDir CMake cache variable (cmake -DpackagesDir=...)")
    message(FATAL_ERROR ${message})
endif()

file(TO_CMAKE_PATH ${PACKAGES_DIR} PACKAGES_DIR)

option(rdepSync
    "Whether rdep should sync packages or use only existing copies"
    ON)

function(nx_rdep_configure)
    set(ENV{RDEP_PACKAGES_DIR} ${PACKAGES_DIR})
    execute_process(COMMAND ${PYTHON_EXECUTABLE} ${rdep_dir}/rdep_configure.py
        RESULT_VARIABLE result)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "Cannot configure RDep repository.")
    endif()

    if(rdepSync)
        execute_process(COMMAND
            ${PYTHON_EXECUTABLE} ${rdep_dir}/rdep.py --sync-timestamps --root ${PACKAGES_DIR})
    endif()
endfunction()

if(targetDevice)
    set(rdep_target ${targetDevice})
else()
    set(rdep_target ${default_target_device})
endif()

if(rdep_target STREQUAL "linux-x64-clang")
    set(rdep_target "linux-x64")
elseif(rdep_target STREQUAL "linux-x86-clang")
    set(rdep_target "linux-x86")
endif()
