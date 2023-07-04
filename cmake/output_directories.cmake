## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

include_guard(GLOBAL)

function(set_output_directories)
    cmake_parse_arguments(DIR "" "RUNTIME;LIBRARY;AFFECTED_VARIABLES_RESULT" "" ${ARGN})

    string(TOUPPER ${CMAKE_BUILD_TYPE} CONFIG)

    set(affected_variables)

    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${DIR_RUNTIME})
    file(MAKE_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY} PARENT_SCOPE)

    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${DIR_LIBRARY})
    file(MAKE_DIRECTORY ${CMAKE_LIBRARY_OUTPUT_DIRECTORY})
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_LIBRARY_OUTPUT_DIRECTORY} PARENT_SCOPE)

    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${CONFIG} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY} PARENT_SCOPE)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${CONFIG} ${CMAKE_LIBRARY_OUTPUT_DIRECTORY} PARENT_SCOPE)

    if(DIR_AFFECTED_VARIABLES_RESULT)
        set(${DIR_AFFECTED_VARIABLES_RESULT}
            CMAKE_RUNTIME_OUTPUT_DIRECTORY
            CMAKE_RUNTIME_OUTPUT_DIRECTORY_${CONFIG}
            CMAKE_LIBRARY_OUTPUT_DIRECTORY
            CMAKE_LIBRARY_OUTPUT_DIRECTORY_${CONFIG}
            PARENT_SCOPE)
    endif()
endfunction()

function(nx_create_dev_qt_conf)
    if(MACOSX OR arch MATCHES "x64|x86")
        set(qt_prefix "${QT_DIR}")
        set(qt_libexec "${CMAKE_BINARY_DIR}/libexec")
    else()
        set(qt_prefix "..")
        set(qt_libexec "libexec")
    endif()

    nx_create_qt_conf(${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/qt.conf
        QT_PREFIX ${qt_prefix}
        QT_LIBEXEC ${qt_libexec})
    nx_create_qt_conf(${CMAKE_BINARY_DIR}/libexec/qt.conf
        QT_PREFIX ${qt_prefix}
        QT_LIBEXEC ${qt_libexec})
endfunction()

set_output_directories(RUNTIME bin LIBRARY lib)

set(distribution_output_dir ${CMAKE_BINARY_DIR}/distrib)
file(MAKE_DIRECTORY ${distribution_output_dir})

set(translations_output_dir ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/translations)
file(MAKE_DIRECTORY ${translations_output_dir})
