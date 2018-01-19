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

function(nx_target_enable_werror target)
    if(${nx_werror_condition})
        target_compile_options(${target} PRIVATE -Werror -Wall -Wextra)
    endif()
endfunction()

function(nx_add_target name type)
    set(options NO_MOC NO_PCH WERROR NO_WERROR)
    set(oneValueArgs LIBRARY_TYPE)
    set(multiValueArgs
        ADDITIONAL_SOURCES ADDITIONAL_RESOURCES
        OTHER_SOURCES
        PUBLIC_LIBS PRIVATE_LIBS)

    cmake_parse_arguments(NX "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    find_sources("${CMAKE_CURRENT_SOURCE_DIR}/src" cpp_files hpp_files)

    set(resources ${NX_ADDITIONAL_RESOURCES})
    if(IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/static-resources")
        list(INSERT resources 0 "${CMAKE_CURRENT_SOURCE_DIR}/static-resources" ${resources})
    endif()

    if(IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/translations")
        file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/qm/translations")
        file(GLOB ts_files "${CMAKE_CURRENT_SOURCE_DIR}/translations/*.ts")

        set_source_files_properties(${ts_files}
            PROPERTIES OUTPUT_LOCATION "${CMAKE_CURRENT_BINARY_DIR}/qm/translations")

        qt5_add_translation(qm_files ${ts_files})

        list(APPEND resources ${qm_files})
        set_source_files_properties(${qm_files}
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
    if(NOT NX_NO_PCH)
        set(sources ${sources} "${CMAKE_CURRENT_SOURCE_DIR}/src/StdAfx.h")
    endif()

    set(sources ${sources} ${NX_ADDITIONAL_SOURCES} ${NX_OTHER_RESOURCES} ${NX_OTHER_SOURCES})

    if(WINDOWS)
        build_source_groups("${CMAKE_CURRENT_SOURCE_DIR}" "${sources}" "src")
    endif()

    if("${type}" STREQUAL "EXECUTABLE")
        set(rc_file)
        if(WINDOWS)
            set(rc_file "${CMAKE_CURRENT_BINARY_DIR}/hdwitness.rc")
            configure_file(
                "${CMAKE_SOURCE_DIR}/cpp/maven/filter-resources/hdwitness.rc"
                "${rc_file}")
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
            project(${name})
            set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${name})
        endif()
    elseif("${type}" STREQUAL "LIBRARY")
        add_library(${name} ${NX_LIBRARY_TYPE} ${sources})
    endif()

    if(stripBinaries)
        nx_strip_target(${name} COPY_DEBUG_INFO)
    endif()

    if(NOT NX_NO_WERROR AND (nx_enable_werror OR NX_WERROR))
        nx_target_enable_werror(${name})
    endif()

    if(NX_NO_MOC)
        set_target_properties(${name} PROPERTIES AUTOMOC OFF)
    else()
        if(NOT NX_NO_PCH)
            set_target_properties(${name} PROPERTIES AUTOMOC_MOC_OPTIONS
                "-b;${CMAKE_CURRENT_SOURCE_DIR}/src/StdAfx.h")
        endif()
    endif()

    if(NOT NX_NO_PCH)
        add_precompiled_header(${name} "${CMAKE_CURRENT_SOURCE_DIR}/src/StdAfx.h" ${pch_flags})
    endif()

    target_include_directories(${name} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")

    if(NX_PUBLIC_LIBS)
        target_link_libraries(${name} PUBLIC ${NX_PUBLIC_LIBS})
    endif()

    if(NX_PRIVATE_LIBS)
        target_link_libraries(${name} PRIVATE ${NX_PRIVATE_LIBS})
    endif()
endfunction()
