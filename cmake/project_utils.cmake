## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

option(enablePrecompiledHeaders "Enable precompiled headers support" ON)
mark_as_advanced(enablePrecompiledHeaders)

set(nx_enable_werror OFF)

include(${PROJECT_SOURCE_DIR}/cmake/windows_signing.cmake)

# build_source_groups(<source root path> <list of source files with absolute paths> <name of root group>)
function(build_source_groups _src_root_path _source_list _root_group_name)
    foreach(_source IN ITEMS ${_source_list})
        get_filename_component(_source_path "${_source}" PATH)
        file(RELATIVE_PATH _source_path_rel "${_src_root_path}" "${_source_path}")
        string(REPLACE "/" "\\" _group_path "${_source_path_rel}")
        string(REPLACE .. ${_root_group_name} result_group_path "${_group_path}")
        source_group("${result_group_path}" FILES "${_source}")
    endforeach()
endfunction()

function(nx_target_enable_werror target werror_condition)
    if(werror_condition STREQUAL "")
        set(werror_condition TRUE)
    endif()

    if(${werror_condition})
        if(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
            target_compile_options(${target} PRIVATE /WX)
        else()
            target_compile_options(${target} PRIVATE -Werror)
        endif()
    endif()
endfunction()

# You need to add NO_API_MACROS option for shared library that doesn't have API macros specified in
# its code for exporting purposes. On Windows such library is forced to be static. On other systems
# if such library is shared then its CXX_VISIBILTY_PRESET is set to default.
function(nx_add_target name type)
    set(options
        NO_MOC
        NO_RC_FILE
        WERROR
        NO_WERROR
        SIGNED
        GDI #< Requires Windows GDI headers to compile
        MACOS_ARG_MAX_WORKAROUND
        NO_API_MACROS)
    set(oneValueArgs LIBRARY_TYPE RC_FILE FOLDER SOURCE_DIR)
    set(multiValueArgs
        ADDITIONAL_SOURCES ADDITIONAL_RESOURCES ADDITIONAL_MOCABLES ADDITIONAL_MOC_INCLUDE_DIRS
        SOURCE_EXCLUSIONS
        OTHER_SOURCES
        FORCE_INCLUDE
        PUBLIC_LIBS PRIVATE_LIBS
        WERROR_IF
    )

    cmake_parse_arguments(NX "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if(NX_SOURCE_DIR)
        set(source_dir "${NX_SOURCE_DIR}")
    else()
        set(source_dir "${CMAKE_CURRENT_SOURCE_DIR}/src")
    endif()

    nx_find_sources("${source_dir}" cpp_files hpp_files
        EXCLUDE ${NX_SOURCE_EXCLUSIONS}
    )

    set(resources ${NX_ADDITIONAL_RESOURCES})
    if(IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/static-resources")
        list(INSERT resources 0 "${CMAKE_CURRENT_SOURCE_DIR}/static-resources")
    endif()

    if(resources)
        nx_generate_qrc(
            ${CMAKE_CURRENT_BINARY_DIR}/${name}.qrc
            ${resources}
            USED_FILES_VARIABLE qrc_dependencies
        )
        nx_add_qrc(
            ${CMAKE_CURRENT_BINARY_DIR}/${name}.qrc
            rcc_files
            DEPENDS ${qrc_dependencies}
        )
    endif()

    set(sources ${cpp_files} ${hpp_files} ${rcc_files} ${qm_files})
    if(EXISTS ${source_dir}/StdAfx.h)
        set(has_pch TRUE)
        list(APPEND sources ${source_dir}/StdAfx.h)
    else()
        set(has_pch FALSE)
    endif()

    set(sources ${sources} ${NX_ADDITIONAL_SOURCES} ${NX_OTHER_RESOURCES} ${NX_OTHER_SOURCES})

    if(WINDOWS)
        build_source_groups("${CMAKE_CURRENT_SOURCE_DIR}" "${sources}" "src")
    endif()

    if("${type}" STREQUAL "EXECUTABLE")
        set(rc_file)
        if(WINDOWS)
            if(NOT NX_NO_RC_FILE)
                if(NX_RC_FILE)
                    set(rc_source_file "${NX_RC_FILE}")
                    set(rc_filename "${NX_RC_FILE}")
                    get_filename_component(rc_filename ${rc_source_file} NAME)
                    set(rc_file "${CMAKE_CURRENT_BINARY_DIR}/${rc_filename}")
                else()
                    find_file(rc_source_file project.rc
                        PATHS
                            "${CMAKE_SOURCE_DIR}/cmake"
                            "${CMAKE_SOURCE_DIR}/open/cmake"
                        REQUIRED)
                    set(rc_file "${CMAKE_CURRENT_BINARY_DIR}/${name}.rc")
                endif()
                configure_file(
                    "${rc_source_file}"
                    "${rc_file}")
                nx_store_known_file(${rc_file})
            endif()
        endif()

        add_executable(${name} ${sources} "${rc_file}")
        set_target_properties(${name} PROPERTIES SKIP_BUILD_RPATH OFF)

        if(WINDOWS)
            # Include "msvc.user.props" into each Visual Studio project.
            set_target_properties(${name} PROPERTIES
                VS_USER_PROPS ${CMAKE_BINARY_DIR}/msvc.user.props)

            # Add user config file to the project dir.
            if((EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${name}.vcxproj.user)
                AND (NOT EXISTS ${CMAKE_CURRENT_BINARY_DIR}/${name}.vcxproj.user))

                nx_configure_file(${CMAKE_CURRENT_SOURCE_DIR}/${name}.vcxproj.user
                    ${CMAKE_CURRENT_BINARY_DIR})
            endif()

            if(codeSigning AND NX_SIGNED)
                nx_sign_windows_executable(${name})
            endif()

            project(${name})

            set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${name})
        endif()
    elseif("${type}" STREQUAL "LIBRARY")
        if(NX_NO_API_MACROS AND WINDOWS)
            if("${NX_LIBRARY_TYPE}" STREQUAL "SHARED")
                message(FATAL_ERROR "Windows library ${name} without API macros can't be SHARED")
            else()
                set(NX_LIBRARY_TYPE STATIC)
            endif()
        endif()
        add_library(${name} ${NX_LIBRARY_TYPE} ${sources})
        if(NX_NO_API_MACROS AND (BUILD_SHARED_LIBS OR "${NX_LIBRARY_TYPE}" STREQUAL "SHARED") AND
            CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
            set_target_properties(${name} PROPERTIES CXX_VISIBILITY_PRESET default)
        endif()
    endif()

    if(MACOSX AND CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
        nx_process_macos_target_debug_symbols(${name})
    endif()

    if(NOT NX_NO_WERROR AND (nx_enable_werror OR NX_WERROR OR NOT "${NX_WERROR_IF}" STREQUAL ""))
        nx_target_enable_werror(${name} "${NX_WERROR_IF}")
    endif()

    # Always run moc for ADDITIONAL_MOCABLES, even if NO_MOC is set.
    if(NX_NO_MOC)
        set(mocable_files ${NX_ADDITIONAL_MOCABLES})
    else()
        set(mocable_files ${hpp_files} ${NX_ADDITIONAL_MOCABLES})
    endif()

    if(NOT NX_NO_MOC OR NX_ADDITIONAL_MOCABLES)
        nx_add_qt_mocables(${name} ${mocable_files}
            INCLUDE_DIRS
                ${source_dir}
                ${NX_ADDITIONAL_MOC_INCLUDE_DIRS}
            FLAGS
                # See moc_nx_reflect_dummy.h for explanation why it is here.
                --include ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/moc_nx_reflect_dummy.h
        )
    endif()

    if(has_pch AND enablePrecompiledHeaders)
        target_precompile_headers(${name} PRIVATE ${source_dir}/StdAfx.h ${NX_FORCE_INCLUDE})

        # Precompile headers are not compatible between languages, turn them off for Objective C.
        foreach(src ${sources})
            if(src MATCHES "^.+\\.mm?$")
                set_source_files_properties("${src}" PROPERTIES SKIP_PRECOMPILE_HEADERS TRUE)
            endif()
        endforeach()
    else()
        nx_force_include(${name} ${NX_FORCE_INCLUDE})
    endif()

    target_include_directories(${name} PUBLIC ${source_dir})

    if(NX_PUBLIC_LIBS)
        target_link_libraries(${name} PUBLIC ${NX_PUBLIC_LIBS})
    endif()

    if(NX_PRIVATE_LIBS)
        target_link_libraries(${name} PRIVATE ${NX_PRIVATE_LIBS})
    endif()

    if(NX_FOLDER)
        set_target_properties(${name} PROPERTIES FOLDER ${NX_FOLDER})
    elseif("${type}" STREQUAL "LIBRARY")
        set_target_properties(${name} PROPERTIES FOLDER libs)
    endif()

    if(WINDOWS)
        nx_store_known_file("${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${name}.pdb")
    endif()

    # Avoids build problems with Clang 15. Should be removed when we update boost library to the
    # newest one.
    add_definitions(-D_LIBCPP_ENABLE_CXX17_REMOVED_UNARY_BINARY_FUNCTION)

    if(WINDOWS AND NOT NX_GDI)
        add_definitions(-DNOGDI=)
    endif()
endfunction()

function(nx_exclude_sources_from_target target pattern)
    get_property(sources TARGET ${target} PROPERTY SOURCES)

    set(result)
    set(matched false)
    foreach(src ${sources})
        if(src MATCHES "${pattern}")
            set(matched true)
        else()
            list(APPEND result ${src})
        endif()
    endforeach()

    if(NOT matched)
        message(FATAL_ERROR
            "No source were excluded from the target ${target} by the pattern \"${pattern}\".\n"
            "Probably some files were recently renamed, deleted or moved to another location.\n"
            "Please adjust the pattern or remove nx_exclude_sources_from_target call.")
    endif()
    set_PROPERTY(TARGET ${target} PROPERTY SOURCES ${result})
endfunction()
