set(nx_enable_werror OFF)
set(nx_werror_condition CMAKE_COMPILER_IS_GNUCXX)

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
        set(werror_condition ${nx_werror_condition})
    endif()

    if(${werror_condition})
        target_compile_options(${target} PRIVATE -Werror)
    endif()
endfunction()

function(nx_add_target name type)
    set(options NO_MOC NO_RC_FILE WERROR NO_WERROR SIGNED MACOS_ARG_MAX_WORKAROUND)
    set(oneValueArgs LIBRARY_TYPE RC_FILE)
    set(multiValueArgs
        ADDITIONAL_SOURCES ADDITIONAL_RESOURCES ADDITIONAL_MOCABLES
        SOURCE_EXCLUSIONS
        OTHER_SOURCES
        PUBLIC_LIBS PRIVATE_LIBS
        WERROR_IF
    )

    cmake_parse_arguments(NX "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    nx_find_sources("${CMAKE_CURRENT_SOURCE_DIR}/src" cpp_files hpp_files
        EXCLUDE ${NX_SOURCE_EXCLUSIONS}
    )

    set(resources ${NX_ADDITIONAL_RESOURCES})
    if(IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/static-resources")
        list(INSERT resources 0 "${CMAKE_CURRENT_SOURCE_DIR}/static-resources")
    endif()

    if(IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/translations")
        file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/qm/translations")
        file(GLOB ts_files "${CMAKE_CURRENT_SOURCE_DIR}/translations/*.ts")

        set_source_files_properties(${ts_files}
            PROPERTIES OUTPUT_LOCATION "${CMAKE_CURRENT_BINARY_DIR}/qm/translations")

        qt5_add_translation(qm_files ${ts_files})

        set(needed_qm_files)
        foreach(qm_file ${qm_files})
            foreach(lang ${translations})
                if(qm_file MATCHES "${lang}\\.qm$")
                    list(APPEND needed_qm_files ${qm_file})
                    break()
                endif()
            endforeach()
        endforeach()

        list(APPEND resources ${needed_qm_files})
        set_source_files_properties(${needed_qm_files}
            PROPERTIES RESOURCE_BASE_DIR "${CMAKE_CURRENT_BINARY_DIR}/qm")
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
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/src/StdAfx.h)
        set(has_pch TRUE)
        list(APPEND sources ${CMAKE_CURRENT_SOURCE_DIR}/src/StdAfx.h)
    else()
        set(has_pch FALSE)
    endif()

    set(sources ${sources} ${NX_ADDITIONAL_SOURCES} ${NX_OTHER_RESOURCES} ${NX_OTHER_SOURCES})

    if(WINDOWS)
        build_source_groups("${CMAKE_CURRENT_SOURCE_DIR}" "${sources}" "src")
    endif()

    if(NX_MACOS_ARG_MAX_WORKAROUND AND CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin")
        # In MacOS CMake incorrectly evauates when command line is too long to pass linker
        # arguments directly. Unfortunately there's no way to enforce passing arguments as a file.
        # This dirty hack overcomes the evaluation limit and forces CMake to use a file.
        set(dummy_directory_name "macos_arg_max_cmake_workaround")
        foreach(i RANGE 10)
            string(APPEND dummy_directory_name _${dummy_directory_name})
        endforeach()
        link_directories(${dummy_directory_name})
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
                    set(rc_source_file "${CMAKE_SOURCE_DIR}/cmake/project.rc")
                    set(rc_file "${CMAKE_CURRENT_BINARY_DIR}/${name}.rc")
                endif()
                configure_file(
                    "${rc_source_file}"
                    "${rc_file}")
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
        add_library(${name} ${NX_LIBRARY_TYPE} ${sources})
    endif()

    if(stripBinaries)
        nx_strip_target(${name} COPY_DEBUG_INFO)
    endif()

    if(NOT NX_NO_WERROR AND (nx_enable_werror OR NX_WERROR OR NOT "${NX_WERROR_IF}" STREQUAL ""))
        nx_target_enable_werror(${name} "${NX_WERROR_IF}")
    endif()

    if(NOT NX_NO_MOC)
        nx_add_qt_mocables(${name} ${hpp_files} ${NX_ADDITIONAL_MOCABLES}
            INCLUDE_DIRS
                ${CMAKE_CURRENT_SOURCE_DIR}/src
                # TODO: #dklychkov Remove hardcoded nx_fusion after updating to a newer Qt which
                # has Q_NAMESPACE macro which can avoid QN_DECLARE_METAOBJECT_HEADER.
                ${CMAKE_SOURCE_DIR}/libs/nx_fusion/src
        )
    endif()

    if(has_pch)
        add_precompiled_header(${name} ${CMAKE_CURRENT_SOURCE_DIR}/src/StdAfx.h)
    endif()

    target_include_directories(${name} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")

    if(NX_PUBLIC_LIBS)
        target_link_libraries(${name} PUBLIC ${NX_PUBLIC_LIBS})
    endif()

    if(NX_PRIVATE_LIBS)
        target_link_libraries(${name} PRIVATE ${NX_PRIVATE_LIBS})
    endif()
endfunction()

function(nx_exclude_sources_from_target target pattern)
    get_property(sources TARGET ${target} PROPERTY SOURCES)

    set(result)
    foreach(src ${sources})
        if(NOT src MATCHES "${pattern}")
            list(APPEND result ${src})
        endif()
    endforeach()

    set_PROPERTY(TARGET ${target} PROPERTY SOURCES ${result})
endfunction()
