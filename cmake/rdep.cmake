set(RDEP_DIR "${CMAKE_SOURCE_DIR}/build_utils/python" CACHE PATH "Path to rdep scripts")
mark_as_advanced(RDEP_DIR)
set(PACKAGES_DIR "$ENV{environment}/packages" CACHE STRING "Path to local rdep repository")
mark_as_advanced(PACKAGES_DIR)
file(TO_CMAKE_PATH "${PACKAGES_DIR}" PACKAGES_DIR)

option(rdepSync
    "Whether rdep should sync packages or use only existing copies"
    ON)

function(nx_rdep_configure)
    execute_process(COMMAND ${PYTHON_EXECUTABLE} ${RDEP_DIR}/rdep_configure.py)
    if(rdepSync)
        execute_process(COMMAND
            ${PYTHON_EXECUTABLE} ${RDEP_DIR}/rdep.py --sync-timestamps --root ${PACKAGES_DIR})
    endif()
endfunction()
