function(nx_add_target name type)
    set(options NO_MOC NO_PCH)
    set(oneValueArgs LIBRARY_TYPE)
    set(multiValueArgs
        ADDITIONAL_SOURCES ADDITIONAL_RESOURCES
        OTHER_SOURCES
        PUBLIC_LIBS PRIVATE_LIBS)

    cmake_parse_arguments(NX "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    find_sources("${CMAKE_CURRENT_SOURCE_DIR}/src" cpp_files hpp_files)
    if(NOT NX_NO_MOC)
        qt5_wrap_cpp(moc_files ${hpp_files} OPTIONS --no-notes)
    endif()

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
        nx_generate_qrc("${CMAKE_CURRENT_BINARY_DIR}/${name}.qrc" ${resources})
        qt5_add_resources(rcc_files "${CMAKE_CURRENT_BINARY_DIR}/${name}.qrc")
    endif()

    set(sources ${cpp_files} ${moc_files} ${rcc_files} ${qm_files})
    if(NOT NX_NO_PCH)
        set(sources ${sources} "${CMAKE_CURRENT_SOURCE_DIR}/src/StdAfx.h")
    endif()

    set(sources ${sources} ${NX_ADDITIONAL_SOURCES} ${NX_OTHER_RESOURCES} ${NX_OTHER_SOURCES})

    if("${type}" STREQUAL "EXECUTABLE")
        add_executable(${name} ${sources})
        set_target_properties(${name} PROPERTIES SKIP_BUILD_RPATH OFF)
    elseif("${type}" STREQUAL "LIBRARY")
        add_library(${name} ${NX_LIBRARY_TYPE} ${sources})
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
