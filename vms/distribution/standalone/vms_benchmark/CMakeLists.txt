set(logs_dir ${CMAKE_BINARY_DIR}/build_logs)
set(build_dir ${CMAKE_CURRENT_BINARY_DIR}/build)
set(zip_file ${distribution_output_dir}/${vms_benchmark_distribution_name}.zip)
set(zip_log ${logs_dir}/vms_benchmark_zip.log)

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
    )
else()
    set(benchmark_python_version 3.6)

    set(dependencies
        ${CMAKE_BINARY_DIR}/lib/libQt5Concurrent.so*
        ${CMAKE_BINARY_DIR}/lib/libQt5Core.so*
        ${CMAKE_BINARY_DIR}/lib/libQt5Gui.so*
        ${CMAKE_BINARY_DIR}/lib/libQt5Network.so*
        ${CMAKE_BINARY_DIR}/lib/libQt5Sql.so*
        ${CMAKE_BINARY_DIR}/lib/libQt5Xml.so*
        ${CMAKE_BINARY_DIR}/lib/libQt5XmlPatterns.so*
        ${CMAKE_BINARY_DIR}/lib/libavcodec.so*
        ${CMAKE_BINARY_DIR}/lib/libavformat.so*
        ${CMAKE_BINARY_DIR}/lib/libavutil.so*
        ${CMAKE_BINARY_DIR}/lib/libcommon.so
        ${CMAKE_BINARY_DIR}/lib/libcrypto.so*
        ${CMAKE_BINARY_DIR}/lib/libicudata.so*
        ${CMAKE_BINARY_DIR}/lib/libicui18n.so*
        ${CMAKE_BINARY_DIR}/lib/libicuuc.so*
        ${CMAKE_BINARY_DIR}/lib/libnx_fusion.so
        ${CMAKE_BINARY_DIR}/lib/libnx_kit.so
        ${CMAKE_BINARY_DIR}/lib/libnx_network.so
        ${CMAKE_BINARY_DIR}/lib/libnx_sql.so
        ${CMAKE_BINARY_DIR}/lib/libnx_utils.so
        ${CMAKE_BINARY_DIR}/lib/libnx_vms_api.so
        ${CMAKE_BINARY_DIR}/lib/libnx_vms_utils.so
        ${CMAKE_BINARY_DIR}/lib/libquazip.so
        ${CMAKE_BINARY_DIR}/lib/libssl.so*
        ${CMAKE_BINARY_DIR}/lib/libstdc++.so.6*
        ${CMAKE_BINARY_DIR}/lib/libswresample.so*
        ${CMAKE_BINARY_DIR}/lib/libswscale.so*
        ${CMAKE_BINARY_DIR}/lib/libudt.so
        ${CMAKE_BINARY_DIR}/bin/rtsp_perf
        ${CMAKE_BINARY_DIR}/bin/testcamera
    )
endif()

# Make one "copy" command for each file to avoid reaching the command line length limit on Windows.
set(copy_dependency_commands)
foreach(dependency ${dependencies})
    file(GLOB dependency_file ${dependency})
    list(APPEND copy_dependency_commands
        COMMAND ${CMAKE_COMMAND} -E copy ${dependency_file} vms_benchmark/testcamera/
    )
endforeach()

add_custom_command(OUTPUT ${zip_file}
    COMMAND ${CMAKE_COMMAND} -E echo "  Copying VMS Benchmark files"
    
    COMMAND ${CMAKE_COMMAND} -E make_directory ${logs_dir}
    COMMAND ${CMAKE_COMMAND} -E remove_directory vms_benchmark
    COMMAND ${CMAKE_COMMAND} -E make_directory vms_benchmark/testcamera
    
    COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_BINARY_DIR}/vms/vms_benchmark/high.ts
        ${CMAKE_BINARY_DIR}/vms/vms_benchmark/low.ts
        ${CMAKE_BINARY_DIR}/bin/vms_benchmark${executable_extension}
        ${CMAKE_BINARY_DIR}/build_info.txt
        ${CMAKE_CURRENT_SOURCE_DIR}/files/vms_benchmark.conf
        ${CMAKE_CURRENT_SOURCE_DIR}/files/readme.md
        vms_benchmark/
 
    ${copy_dependency_commands}
 
    COMMAND ${CMAKE_COMMAND} -E echo "  Zipping VMS Benchmark - see ${zip_log}"
    COMMAND ${CMAKE_COMMAND} -E tar cvf ${zip_file} --format=zip vms_benchmark >${zip_log}

    DEPENDS vms_benchmark
)

add_custom_target(distribution_vms_benchmark ALL DEPENDS ${zip_file})
