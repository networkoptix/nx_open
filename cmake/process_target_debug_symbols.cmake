## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

function(nx_process_macos_target_debug_symbols target)
    get_target_property(type ${target} TYPE)
    if(NOT type MATCHES "MODULE_LIBRARY|SHARED_LIBRARY|EXECUTABLE")
        return()
    endif()

    if(NOT CMAKE_STRIP)
        message(FATAL_ERROR "strip is not found.")
    endif()

    if(developerBuild)
        # Allow Xcode to find dSYM files next to the binaries.
        set(nativeSymbolsDir "$<TARGET_FILE_DIR:${target}>")
    else()
        # Put dSYM files into a different directory to avoid signing issues.
        set(nativeSymbolsDir "${CMAKE_BINARY_DIR}")
    endif()

    set(DSYM_PATH "${nativeSymbolsDir}/$<TARGET_FILE_NAME:${target}>.dSYM")

    set(create_dsym_command COMMAND dsymutil "$<TARGET_FILE:${target}>" -o "${DSYM_PATH}")

    set(create_breakpad_sym_command COMMAND
        "${PYTHON_EXECUTABLE}" "${CONAN_BREAKPAD-TOOLS_ROOT}/generate_breakpad_symbols.py" --dump-syms-path "${CONAN_BREAKPAD-TOOLS_ROOT}/bin/dump_syms" --build-dir "${PROJECT_BINARY_DIR}" --symbols-dir "${breakpadSymbolsOutputDir}" --binary "$<TARGET_FILE:${target}>" --dsym-path "${DSYM_PATH}" --skip-deps --verbose)

    add_custom_command(TARGET ${target} POST_BUILD
        ${create_dsym_command}
        ${create_breakpad_sym_command}
        COMMAND ${CMAKE_STRIP} -S "$<TARGET_FILE:${target}>"
        COMMENT "Generating debug symbols and stripping ${target}")
endfunction()

function(nx_process_linux_target_debug_symbols target)
    get_target_property(type ${target} TYPE)
    if(NOT type MATCHES "MODULE_LIBRARY|SHARED_LIBRARY|EXECUTABLE")
        return()
    endif()

    if(NOT CMAKE_STRIP)
        message(FATAL_ERROR "strip is not found.")
    endif()

    if(NOT CMAKE_OBJCOPY)
        message(FATAL_ERROR "objcopy is not found.")
    endif()

    # Use `settings set target.debug-file-search-paths ${nativeSymbolsDir}` in lldb
    # to set the search path for debug files.
    set(nativeSymbolsDir "${CMAKE_BINARY_DIR}")

    # When building Android app on macOS, use the dumps_syms which handles elf files.
    if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin")
        set(DUMP_SYMS "${CONAN_BREAKPAD-TOOLS_ROOT}/bin/dump_syms_linux")
    else()
        set(DUMP_SYMS "${CONAN_BREAKPAD-TOOLS_ROOT}/bin/dump_syms")
    endif()

    if(targetDevice STREQUAL "android_arm32" AND CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin")
        # On macOS the dump_syms_linux is built only for 64-bit elf files.
        set(DUMP_SYMS_COMMAND)
    else()
        # Android dependencies contain debug symbols that also need to be processed.
        # Stripping debug symbols is handled in Gradle.
        if(ANDROID)
            get_filename_component(COMPILER_DIR "${CMAKE_CXX_COMPILER}" DIRECTORY)
            set(GEN_SYM_ARGS "--platform=android" "--readelf-path=${COMPILER_DIR}/llvm-readelf" "--lib-paths=${QT_DIR}/lib")
        else()
            set(GEN_SYM_ARGS "--skip-deps")
        endif()
        set(DUMP_SYMS_COMMAND COMMAND
            "${PYTHON_EXECUTABLE}" "${CONAN_BREAKPAD-TOOLS_ROOT}/generate_breakpad_symbols.py" --dump-syms-path "${DUMP_SYMS}" --build-dir "${PROJECT_BINARY_DIR}" --symbols-dir "${breakpadSymbolsOutputDir}" --binary "$<TARGET_FILE:${target}>" --verbose ${GEN_SYM_ARGS})
    endif()

    add_custom_command(TARGET ${target} POST_BUILD
        ${DUMP_SYMS_COMMAND}
        COMMAND "${CMAKE_OBJCOPY}" --only-keep-debug "$<TARGET_FILE:${target}>" "${nativeSymbolsDir}/$<TARGET_FILE_NAME:${target}>.debug"
        COMMAND "${CMAKE_OBJCOPY}" --add-gnu-debuglink "$<TARGET_FILE_NAME:${target}>.debug" "$<TARGET_FILE:${target}>"
        COMMAND "${CMAKE_STRIP}" -S "$<TARGET_FILE:${target}>"
        WORKING_DIRECTORY "${nativeSymbolsDir}" # For the --add-gnu-debuglink command to work correctly.
        COMMENT "Generating debug symbols and stripping ${target}")
endfunction()

function(nx_process_windows_target_debug_symbols target)
    get_target_property(type ${target} TYPE)
    if(NOT type MATCHES "MODULE_LIBRARY|SHARED_LIBRARY|EXECUTABLE")
        return()
    endif()

    set(DUMP_SYMS "${CONAN_BREAKPAD-TOOLS_ROOT}/bin/dump_syms.exe")
    set(DUMP_SYMS_COMMAND COMMAND
        "${PYTHON_EXECUTABLE}" "${CONAN_BREAKPAD-TOOLS_ROOT}/generate_breakpad_symbols.py" --dump-syms-path "${DUMP_SYMS}" --build-dir "${PROJECT_BINARY_DIR}" --symbols-dir "${breakpadSymbolsOutputDir}" --binary "$<TARGET_FILE:${target}>" --skip-deps --verbose)

    add_custom_command(TARGET ${target} POST_BUILD
        ${DUMP_SYMS_COMMAND}
        WORKING_DIRECTORY "${DEBUG_OUTPUT_DIR}"
        COMMENT "Generating debug symbols for ${target}")
endfunction()
