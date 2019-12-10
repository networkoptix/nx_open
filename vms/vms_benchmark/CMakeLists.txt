message(STATUS "Generating files for VMS Benchmark")

set(build_dir ${CMAKE_CURRENT_BINARY_DIR}/build)
set(vms_version ${releaseVersion.full})

nx_copy(${CMAKE_CURRENT_SOURCE_DIR}/release DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
nx_configure_file(
    ${CMAKE_CURRENT_BINARY_DIR}/release/lib/vms_benchmark_release/version.py.in
    ${CMAKE_CURRENT_BINARY_DIR}/release/lib/vms_benchmark_release/version.py
)

file(GLOB_RECURSE benchmark_sources ${CMAKE_CURRENT_SOURCE_DIR}/*.py ${CMAKE_CURRENT_BINARY_DIR}/release/lib/*.py)

if(targetDevice STREQUAL "windows-x64")
    set(executable_extension .exe)
    set(bash_command ${vms_benchmark_dev_dir}/buildtools/sh.exe)
    set(benchmark_python_version 3.7)
else()
    set(bash_command bash)
    set(benchmark_python_version 3.6)
endif()

list(JOIN benchmark_dependencies_list " " benchmark_dependencies)

# Command to be executed at the build phase.
add_custom_command(OUTPUT
        ${CMAKE_BINARY_DIR}/bin/vms_benchmark${executable_extension}
        ${CMAKE_BINARY_DIR}/bin/vms_benchmark.conf
        ${CMAKE_BINARY_DIR}/bin/vms_benchmark.ini
    VERBATIM
    COMMAND ${CMAKE_COMMAND} -E make_directory ${build_dir}

    COMMAND ${CMAKE_COMMAND} -E echo "  Building VMS Benchmark"

    COMMAND ${CMAKE_COMMAND} -E copy
        ${vms_benchmark_dev_dir}/zip/high.ts
        ${vms_benchmark_dev_dir}/zip/low.ts
        ${CMAKE_CURRENT_BINARY_DIR}/

    COMMAND ${bash_command} -c " \
        export PATH=\"${vms_benchmark_dev_dir}/python/bin:${vms_benchmark_dev_dir}/python:/usr/bin:/bin\"; \
        export LD_LIBRARY_PATH=\"${vms_benchmark_dev_dir}/python/lib\"; \
        export PYTHONPATH=\"${vms_benchmark_dev_dir}/python/lib/python${benchmark_python_version}/site-packages\"; \
        python3${executable_extension} \
        -m PyInstaller \
        -y \
        -F \
        --hidden-import json \
        --distpath \"${build_dir}/distrib\" \
        --workpath \"${build_dir}/build\" \
        --specpath \"${build_dir}\" \
        -p \"${CMAKE_CURRENT_SOURCE_DIR}/lib\" \
        -p \"${CMAKE_CURRENT_BINARY_DIR}/release/lib\" \
        --name vms_benchmark \
        \"${CMAKE_CURRENT_SOURCE_DIR}/bin/main.py\" \
    "

    COMMAND ${CMAKE_COMMAND} -E copy
        ${build_dir}/distrib/vms_benchmark${executable_extension}
        ${CMAKE_CURRENT_SOURCE_DIR}/vms_benchmark.conf
        ${CMAKE_CURRENT_SOURCE_DIR}/vms_benchmark.ini
        ${CMAKE_BINARY_DIR}/bin/

    COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_CURRENT_SOURCE_DIR}/vms_benchmark.builddir.ini
        ${CMAKE_BINARY_DIR}/bin/vms_benchmark.ini

    DEPENDS
        testcamera
        rtsp_perf
        ${benchmark_sources}
)

add_custom_target(vms_benchmark ALL DEPENDS ${CMAKE_BINARY_DIR}/bin/vms_benchmark${executable_extension})
set_target_properties(vms_benchmark PROPERTIES FOLDER utils)