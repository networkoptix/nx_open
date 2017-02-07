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
        qt5_wrap_cpp(moc_files ${hpp_files})
    endif()

    set(RESOURCES ${NX_ADDITIONAL_RESOURCES})
    if(IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/static-resources")
        set(RESOURCES "${CMAKE_CURRENT_SOURCE_DIR}/static-resources" ${RESOURCES})
    endif()
    if(RESOURCES)
        generate_qrc("${CMAKE_CURRENT_BINARY_DIR}/${name}.qrc" ${RESOURCES})
        qt5_add_resources(rcc_files "${CMAKE_CURRENT_BINARY_DIR}/${name}.qrc")
    endif()

    set(sources ${cpp_files} ${moc_files} ${rcc_files})
    if(NOT NX_NO_PCH)
        set(sources ${sources}
            "${CMAKE_CURRENT_SOURCE_DIR}/src/StdAfx.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/StdAfx.h")
    endif()

    set(sources ${sources} ${NX_ADDITIONAL_SOURCES} ${NX_OTHER_RESOURCES} ${NX_OTHER_SOURCES})

    if("${type}" STREQUAL "EXECUTABLE")
        add_executable(${name} ${sources})
    elseif("${type}" STREQUAL "LIBRARY")
        add_library(${name} ${NX_LIBRARY_TYPE} ${sources})
    endif()

    if(NOT NX_NO_PCH)
        add_precompiled_header(${name} "${CMAKE_CURRENT_SOURCE_DIR}/src/StdAfx.h" FORCEINCLUDE)
    endif()

    target_include_directories(${name} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")

    if(NX_PUBLIC_LIBS)
        target_link_libraries(${name} PUBLIC ${NX_PUBLIC_LIBS})
    endif()

    if(NX_PRIVATE_LIBS)
        target_link_libraries(${name} PRIVATE ${NX_PUBLIC_LIBS})
    endif()
endfunction()
