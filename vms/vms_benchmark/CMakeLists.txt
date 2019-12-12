message(STATUS "Generating files for VMS Benchmark")

set(build_dir ${CMAKE_CURRENT_BINARY_DIR}/build)
set(vms_version ${releaseVersion.full})

nx_copy(${CMAKE_CURRENT_SOURCE_DIR}/release DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
nx_configure_file(
    ${CMAKE_CURRENT_BINARY_DIR}/release/lib/vms_benchmark_release/version.py.in
    ${CMAKE_CURRENT_BINARY_DIR}/release/lib/vms_benchmark_release/version.py
)

file(GLOB_RECURSE benchmark_sources
    ${CMAKE_CURRENT_SOURCE_DIR}/*.py ${CMAKE_CURRENT_BINARY_DIR}/release/lib/*.py)

set(py_installer_args
    -m PyInstaller
    -y
    -F
    --hidden-import json
    --distpath \"${build_dir}/distrib\"
    --workpath \"${build_dir}/build\"
    --specpath \"${build_dir}\"
    -p \"${CMAKE_CURRENT_SOURCE_DIR}/lib\"
    -p \"${CMAKE_CURRENT_BINARY_DIR}/release/lib\"
    --name vms_benchmark
    \"${CMAKE_CURRENT_SOURCE_DIR}/bin/main.py\"
)
list(JOIN py_installer_args " " py_installer_args_command_line)

set(python_dir "${vms_benchmark_dev_dir}/python")

if(targetDevice STREQUAL "windows-x64")
    set(exe_extension ".exe")

    set(py_installer_with_env_command
        COMMAND cmd /C
        " \
            \" \
                set PYTHONPATH=\"${python_dir}/lib/python3.7/site-packages\" && \
                ${python_dir}/python3.exe ${py_installer_args_command_line} \
            \" \
        "
    )
else()
    set(exe_extension "")

    # NOTE: add_custom_command() on Linux requires each quote and ampersand to be prepended with
    # a bashslash.
    
    string(REPLACE "\"" "\\\""
        py_installer_args_command_line_escaped_quotes "${py_installer_args_command_line}")
        
    set(py_installer_with_env_command
        COMMAND bash -c
        " \
            export PATH=\\\"${python_dir}/bin:/usr/bin:/bin\\\" \\&\\& \
            export LD_LIBRARY_PATH=\\\"${python_dir}/lib\\\" \\&\\& \
            export PYTHONPATH=\\\"${python_dir}/lib/python3.6/site-packages\\\" \\&\\& \
            python3 ${py_installer_args_command_line_escaped_quotes} \
        "
    )
endif()

# Command to be executed at the build phase.
add_custom_command(
    OUTPUT
        ${CMAKE_BINARY_DIR}/bin/vms_benchmark${exe_extension}
        ${CMAKE_BINARY_DIR}/bin/vms_benchmark.conf
        ${CMAKE_BINARY_DIR}/bin/vms_benchmark.ini
    
    COMMAND ${CMAKE_COMMAND} -E echo "  Building VMS Benchmark executable"

    COMMAND ${CMAKE_COMMAND} -E make_directory ${build_dir}

    COMMAND ${CMAKE_COMMAND} -E copy
        ${vms_benchmark_dev_dir}/zip/high.ts
        ${vms_benchmark_dev_dir}/zip/low.ts
        ${CMAKE_CURRENT_BINARY_DIR}/

    ${py_installer_with_env_command}

    COMMAND ${CMAKE_COMMAND} -E copy
        ${build_dir}/distrib/vms_benchmark${exe_extension}
        ${CMAKE_CURRENT_SOURCE_DIR}/vms_benchmark.conf
        ${CMAKE_BINARY_DIR}/bin/

    COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_CURRENT_SOURCE_DIR}/vms_benchmark.builddir.ini
        ${CMAKE_BINARY_DIR}/bin/vms_benchmark.ini

    DEPENDS
        testcamera
        rtsp_perf
        ${benchmark_sources}
)

add_custom_target(vms_benchmark ALL DEPENDS ${CMAKE_BINARY_DIR}/bin/vms_benchmark${exe_extension})
set_target_properties(vms_benchmark PROPERTIES FOLDER utils)