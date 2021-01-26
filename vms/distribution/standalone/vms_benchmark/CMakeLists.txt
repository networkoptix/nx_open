set(logs_dir ${CMAKE_BINARY_DIR}/build_logs)
set(build_dir ${CMAKE_CURRENT_BINARY_DIR}/build)
set(zip_file ${distribution_output_dir}/${vms_benchmark_distribution_name}.zip)
set(zip_log ${logs_dir}/vms_benchmark_zip.log)

# Generated commands for add_custom_command() which creates subdirectories - platform-dependent.
set(create_subdirectories_commands)

# Generated commands for add_custom_command() which copies the files - platform-dependent.
set(copy_dependencies_commands)

function(add_dependencies_to_copy_command dependencies destination_dir)
    # Assuming dependencies do not contain spaces.
    list(JOIN dependencies " " dependencies_args)

    list(APPEND copy_dependencies_commands
        # Call `cp` via bash to resolve `*` used for .so symlinks.
        COMMAND bash -c
            "cp -r ${dependencies_args} vms_benchmark/tools/${destination_dir}"
    )

    set(copy_dependencies_commands ${copy_dependencies_commands} PARENT_SCOPE)
endfunction()

if(targetDevice STREQUAL "windows-x64")
    set(executable_extension .exe)
    set(benchmark_python_version 3.7)

    set(dependencies
        ${ucrt_directory}/bin/api-ms-win-core-console-l1-1-0.dll
        ${ucrt_directory}/bin/api-ms-win-core-datetime-l1-1-0.dll
        ${ucrt_directory}/bin/api-ms-win-core-debug-l1-1-0.dll
        ${ucrt_directory}/bin/api-ms-win-core-errorhandling-l1-1-0.dll
        ${ucrt_directory}/bin/api-ms-win-core-file-l1-1-0.dll
        ${ucrt_directory}/bin/api-ms-win-core-file-l1-2-0.dll
        ${ucrt_directory}/bin/api-ms-win-core-file-l2-1-0.dll
        ${ucrt_directory}/bin/api-ms-win-core-handle-l1-1-0.dll
        ${ucrt_directory}/bin/api-ms-win-core-heap-l1-1-0.dll
        ${ucrt_directory}/bin/api-ms-win-core-interlocked-l1-1-0.dll
        ${ucrt_directory}/bin/api-ms-win-core-libraryloader-l1-1-0.dll
        ${ucrt_directory}/bin/api-ms-win-core-localization-l1-2-0.dll
        ${ucrt_directory}/bin/api-ms-win-core-memory-l1-1-0.dll
        ${ucrt_directory}/bin/api-ms-win-core-namedpipe-l1-1-0.dll
        ${ucrt_directory}/bin/api-ms-win-core-processenvironment-l1-1-0.dll
        ${ucrt_directory}/bin/api-ms-win-core-processthreads-l1-1-0.dll
        ${ucrt_directory}/bin/api-ms-win-core-processthreads-l1-1-1.dll
        ${ucrt_directory}/bin/api-ms-win-core-profile-l1-1-0.dll
        ${ucrt_directory}/bin/api-ms-win-core-rtlsupport-l1-1-0.dll
        ${ucrt_directory}/bin/api-ms-win-core-string-l1-1-0.dll
        ${ucrt_directory}/bin/api-ms-win-core-synch-l1-1-0.dll
        ${ucrt_directory}/bin/api-ms-win-core-synch-l1-2-0.dll
        ${ucrt_directory}/bin/api-ms-win-core-sysinfo-l1-1-0.dll
        ${ucrt_directory}/bin/api-ms-win-core-timezone-l1-1-0.dll
        ${ucrt_directory}/bin/api-ms-win-core-util-l1-1-0.dll
        ${ucrt_directory}/bin/api-ms-win-crt-conio-l1-1-0.dll
        ${ucrt_directory}/bin/api-ms-win-crt-convert-l1-1-0.dll
        ${ucrt_directory}/bin/api-ms-win-crt-environment-l1-1-0.dll
        ${ucrt_directory}/bin/api-ms-win-crt-filesystem-l1-1-0.dll
        ${ucrt_directory}/bin/api-ms-win-crt-heap-l1-1-0.dll
        ${ucrt_directory}/bin/api-ms-win-crt-locale-l1-1-0.dll
        ${ucrt_directory}/bin/api-ms-win-crt-math-l1-1-0.dll
        ${ucrt_directory}/bin/api-ms-win-crt-multibyte-l1-1-0.dll
        ${ucrt_directory}/bin/api-ms-win-crt-private-l1-1-0.dll
        ${ucrt_directory}/bin/api-ms-win-crt-process-l1-1-0.dll
        ${ucrt_directory}/bin/api-ms-win-crt-runtime-l1-1-0.dll
        ${ucrt_directory}/bin/api-ms-win-crt-stdio-l1-1-0.dll
        ${ucrt_directory}/bin/api-ms-win-crt-string-l1-1-0.dll
        ${ucrt_directory}/bin/api-ms-win-crt-time-l1-1-0.dll
        ${ucrt_directory}/bin/api-ms-win-crt-utility-l1-1-0.dll
        ${ucrt_directory}/bin/ucrtbase.dll
        ${vcrt_directory}/bin/concrt140.dll
        ${vcrt_directory}/bin/vccorlib140.dll
        ${vcrt_directory}/bin/vcruntime140.dll
        ${vcrt_directory}/bin/msvcp140.dll
        ${vcrt_directory}/bin/msvcp140_1.dll
        ${vcrt_directory}/bin/msvcp140_2.dll
        ${QT_DIR}/bin/Qt5Core.dll
        ${QT_DIR}/bin/Qt5Gui.dll
        ${QT_DIR}/bin/Qt5Multimedia.dll
        ${QT_DIR}/bin/Qt5Network.dll
        ${QT_DIR}/bin/Qt5Sql.dll
        ${QT_DIR}/bin/Qt5Xml.dll
        ${QT_DIR}/bin/icudt60.dll
        ${QT_DIR}/bin/icuin60.dll
        ${QT_DIR}/bin/icuuc60.dll
        ${CMAKE_BINARY_DIR}/bin/avcodec-57.dll
        ${CMAKE_BINARY_DIR}/bin/avformat-57.dll
        ${CMAKE_BINARY_DIR}/bin/avutil-55.dll
        ${CMAKE_BINARY_DIR}/bin/libeay32.dll
        ${CMAKE_BINARY_DIR}/bin/nx_kit.dll
        ${CMAKE_BINARY_DIR}/bin/nx_network.dll
        ${CMAKE_BINARY_DIR}/bin/nx_utils.dll
        ${CMAKE_BINARY_DIR}/bin/nx_vms_api.dll
        ${CMAKE_BINARY_DIR}/bin/nx_vms_utils.dll
        ${CMAKE_BINARY_DIR}/bin/ssleay32.dll
        ${CMAKE_BINARY_DIR}/bin/swresample-2.dll
        ${CMAKE_BINARY_DIR}/bin/swscale-4.dll
        ${CMAKE_BINARY_DIR}/bin/udt.dll
        ${CMAKE_BINARY_DIR}/bin/rtsp_perf.exe
        ${CMAKE_BINARY_DIR}/bin/testcamera.exe
        ${RDEP_VMS_BENCHMARK-DEV_ROOT}/files/plink.exe
    )

    list(APPEND create_subdirectories_commands
        COMMAND ${CMAKE_COMMAND} -E make_directory vms_benchmark/tools/bin
    )

    # Make one "copy" command for each file to avoid reaching the command line length limit.
    foreach(dependency ${dependencies})
        list(APPEND copy_dependencies_commands
            COMMAND
                ${CMAKE_COMMAND} -E copy ${dependency} vms_benchmark/tools/bin/
        )
    endforeach()
else()
    set(benchmark_python_version 3.6)

    set(lib_dependencies
        ${CMAKE_BINARY_DIR}/lib/libQt5Concurrent.so*
        ${CMAKE_BINARY_DIR}/lib/libQt5Core.so*
        ${CMAKE_BINARY_DIR}/lib/libQt5Gui.so*
        ${CMAKE_BINARY_DIR}/lib/libQt5Network.so*
        ${CMAKE_BINARY_DIR}/lib/libQt5Sql.so*
        ${CMAKE_BINARY_DIR}/lib/libQt5Xml.so*
        ${CMAKE_BINARY_DIR}/lib/libavcodec.so*
        ${CMAKE_BINARY_DIR}/lib/libavformat.so*
        ${CMAKE_BINARY_DIR}/lib/libavutil.so*
        ${CMAKE_BINARY_DIR}/lib/libcommon.so
        ${CMAKE_BINARY_DIR}/lib/libcrypto.so*
        ${CMAKE_BINARY_DIR}/lib/libnx_fusion.so
        ${CMAKE_BINARY_DIR}/lib/libnx_kit.so
        ${CMAKE_BINARY_DIR}/lib/libnx_network.so
        ${CMAKE_BINARY_DIR}/lib/libnx_sql.so
        ${CMAKE_BINARY_DIR}/lib/libnx_utils.so
        ${CMAKE_BINARY_DIR}/lib/libnx_vms_api.so
        ${CMAKE_BINARY_DIR}/lib/libnx_vms_utils.so
        ${CMAKE_BINARY_DIR}/lib/libquazip.so
        ${CMAKE_BINARY_DIR}/lib/libssl.so*
        ${CMAKE_BINARY_DIR}/lib/libswresample.so*
        ${CMAKE_BINARY_DIR}/lib/libswscale.so*
        ${CMAKE_BINARY_DIR}/lib/libudt.so
        ${CMAKE_BINARY_DIR}/lib/libgl_stub/libGL.so.1
    )

    set(bin_dependencies
        ${CMAKE_BINARY_DIR}/bin/rtsp_perf
        ${CMAKE_BINARY_DIR}/bin/testcamera
    )

    list(APPEND create_subdirectories_commands
        COMMAND ${CMAKE_COMMAND} -E make_directory vms_benchmark/tools/bin
        COMMAND ${CMAKE_COMMAND} -E make_directory vms_benchmark/tools/lib
    )

    add_dependencies_to_copy_command("${lib_dependencies}" lib)
    add_dependencies_to_copy_command("${bin_dependencies}" bin)

    set(system_dependencies
        ${cpp_runtime_libs}
        ${icu_runtime_libs}
    )

    list(APPEND copy_dependencies_commands
        COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_SOURCE_DIR}/build_utils/linux/copy_system_library.py
            --compiler=${CMAKE_CXX_COMPILER}
            --flags=${CMAKE_CXX_FLAGS}
            --link-flags=${CMAKE_SHARED_LINKER_FLAGS}
            --dest-dir=vms_benchmark/tools/lib
            ${system_dependencies}
    )
endif()

add_custom_command(OUTPUT ${zip_file}
    COMMAND ${CMAKE_COMMAND} -E echo "  Copying VMS Benchmark files"

    COMMAND ${CMAKE_COMMAND} -E make_directory ${logs_dir}
    COMMAND ${CMAKE_COMMAND} -E remove_directory vms_benchmark
    COMMAND ${CMAKE_COMMAND} -E make_directory vms_benchmark

    COMMAND ${CMAKE_COMMAND} -E copy
        ${RDEP_VMS_BENCHMARK_AUX_FILES_ROOT}/files/high.ts
        ${RDEP_VMS_BENCHMARK_AUX_FILES_ROOT}/files/low.ts
        ${CMAKE_BINARY_DIR}/bin/vms_benchmark${executable_extension}
        ${distribution_output_dir}/build_info.txt
        ${distribution_output_dir}/build_info.json
        ${CMAKE_CURRENT_SOURCE_DIR}/files/vms_benchmark.conf
        ${CMAKE_CURRENT_SOURCE_DIR}/files/readme.md
        vms_benchmark/

    ${create_subdirectories_commands}
    ${copy_dependencies_commands}

    COMMAND ${CMAKE_COMMAND} -E echo "  Zipping VMS Benchmark - see ${zip_log}"
    COMMAND ${CMAKE_COMMAND} -E tar cvf ${zip_file} --format=zip vms_benchmark >${zip_log}

    DEPENDS
        vms_benchmark
        testcamera
        rtsp_perf
)

add_custom_target(distribution_vms_benchmark ALL DEPENDS ${zip_file})
