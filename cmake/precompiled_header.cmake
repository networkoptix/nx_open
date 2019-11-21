option(enablePrecompiledHeaders "Enable precompiled headers support" ON)
mark_as_advanced(enablePrecompiledHeaders)

function(_nx_create_compiler_id_file file_name)
    configure_file(${CMAKE_SOURCE_DIR}/cmake/compiler_id.in ${file_name})
endfunction()

function(_generate_pch_parameters target parameters_file)
    set(flags
        "$<TARGET_PROPERTY:${target},COMPILE_OPTIONS>"
        "$<$<BOOL:$<TARGET_PROPERTY:${target},POSITION_INDEPENDENT_CODE>>:${CMAKE_CXX_COMPILE_OPTIONS_PIC}>"
        ${CMAKE_CXX_FLAGS})

    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        list(APPEND flags ${CMAKE_CXX_FLAGS_DEBUG})
    elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
        list(APPEND flags ${CMAKE_CXX_FLAGS_RELEASE})
    elseif(CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
        list(APPEND flags ${CMAKE_CXX_FLAGS_RELWITHDEBINFO})
    elseif(CMAKE_BUILD_TYPE STREQUAL "MinSizeRel")
        list(APPEND flags ${CMAKE_CXX_FLAGS_MINSIZEREL})
    endif()

    if(CMAKE_CXX_COMPILER_TARGET)
        list(INSERT flags 0 ${CMAKE_CXX_COMPILE_OPTIONS_TARGET}${CMAKE_CXX_COMPILER_TARGET})
    endif()

    if(MACOSX AND CMAKE_OSX_SYSROOT MATCHES "^/.+$")
        list(APPEND flags "-isysroot ${CMAKE_OSX_SYSROOT}")
    endif()

    set(flags "$<$<BOOL:${flags}>:$<JOIN:${flags},\n>\n>")

    set(include_directories
        "$<TARGET_PROPERTY:${target},INCLUDE_DIRECTORIES>"
        ${CMAKE_CXX_STANDARD_INCLUDE_DIRECTORIES})
    set(include_directories
        "$<$<BOOL:${include_directories}>:-I$<JOIN:${include_directories},\n-I>\n>")

    if(MACOSX)
        set(framework_directories
            "$<$<BOOL:${CMAKE_FRAMEWORK_PATH}>:-iframework $<JOIN:${CMAKE_FRAMEWORK_PATH},\n-iframework >\n>")
    else()
        set(framework_directories)
    endif()

    set(definitions
        "$<TARGET_PROPERTY:${target},COMPILE_DEFINITIONS>")
    set(definitions
        "$<$<BOOL:${definitions}>:-D$<JOIN:${definitions},\n-D>\n>")

    file(GENERATE
        OUTPUT "${parameters_file}"
        CONTENT "${flags}${include_directories}${framework_directories}${definitions}\n"
    )
endfunction()

function(_get_cxx_standard target STANDARD_VAR)
    get_property(standard TARGET ${target} PROPERTY CXX_STANDARD)
    if(standard STREQUAL "98")
        set(standard "${CMAKE_CXX98_STANDARD_COMPILE_OPTION}")
    elseif(standard STREQUAL "11")
        set(standard "${CMAKE_CXX11_STANDARD_COMPILE_OPTION}")
    elseif(standard STREQUAL "14")
        set(standard "${CMAKE_CXX14_STANDARD_COMPILE_OPTION}")
    elseif(standard STREQUAL "17")
        set(standard "${CMAKE_CXX17_STANDARD_COMPILE_OPTION}")
    else()
        unset(standard)
    endif()

    set(${STANDARD_VAR} ${standard} PARENT_SCOPE)
endfunction()

function(_add_msvc_precompiled_header target input)
    get_filename_component(pch_src_dir "${input}" DIRECTORY)
    get_filename_component(pch_name_we "${input}" NAME_WE)
    set(pch_cpp "${pch_src_dir}/${pch_name_we}.cpp")

    if(NOT EXISTS "${pch_cpp}")
        message(FATAL_ERROR "Precompiled header source file does not exist: ${pch_cpp}")
    endif()

    get_filename_component(pch_name "${input}" NAME)
    set(pch_file "${CMAKE_CFG_INTDIR}/${target}.pch")

    set_property(TARGET "${target}"
        APPEND PROPERTY COMPILE_FLAGS "/FI${pch_name} /Yu${pch_name} \"/Fp${pch_file}\"")
    set_property(TARGET ${target}
        APPEND PROPERTY SOURCES "${pch_cpp}")
    set_property(SOURCE "${pch_cpp}"
        APPEND PROPERTY COMPILE_FLAGS "/Yc \"/FI$(NOINHERIT)\"")
endfunction()

function(_add_xcode_precompiled_header target input)
    set_target_properties(${target} PROPERTIES XCODE_ATTRIBUTE_GCC_PRECOMPILE_PREFIX_HEADER "YES")
    set_target_properties(${target} PROPERTIES XCODE_ATTRIBUTE_GCC_PREFIX_HEADER "${input}")
endfunction()

function(_add_gcc_clang_precompiled_header target input)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        set(pch_dir "${CMAKE_CURRENT_BINARY_DIR}/${target}.pch")
    else()
        set(pch_dir "${CMAKE_CURRENT_BINARY_DIR}/${target}.gch")
    endif()
    set(pch_file "${pch_dir}/c++")

    set(parameters_file "${pch_dir}_${CMAKE_CXX_COMPILER_VERSION}.parameters")
    _generate_pch_parameters(${target} ${parameters_file})
    _get_cxx_standard(${target} cxx_standard)

    file(MAKE_DIRECTORY "${pch_dir}")

    set(depfile_args)
    set(depfile_cmd_args)
    if("${CMAKE_GENERATOR}" STREQUAL "Ninja")
        set(depfile "${pch_dir}.d")
        file(RELATIVE_PATH pch_file_relative "${CMAKE_BINARY_DIR}" "${pch_file}")

        set(depfile_args DEPFILE "${depfile}")
        set(depfile_cmd_args -MD -MF "${depfile}" -MT "${pch_file_relative}")
    endif()

    set(compiler_id_file ${CMAKE_CURRENT_BINARY_DIR}/compiler_id)
    _nx_create_compiler_id_file(${compiler_id_file})

    add_custom_command(
        OUTPUT "${pch_file}"
        COMMAND "${CMAKE_CXX_COMPILER}"
            "@${parameters_file}" ${cxx_standard} -x c++-header "${input}" -o "${pch_file}"
            ${depfile_cmd_args}
        DEPENDS "${input}" "${parameters_file}" ${compiler_id_file}
        IMPLICIT_DEPENDS CXX "${input}"
        ${depfile_args}
        COMMENT "Precompiling ${pch_dir}")

    set(pch_flags -Winvalid-pch)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        list(APPEND pch_flags -Xclang -include-pch -Xclang ${pch_file})
    else()
        list(APPEND pch_flags -include ${CMAKE_CURRENT_BINARY_DIR}/${target})
    endif()

    nx_get_target_cpp_sources(${target} sources)
    set_property(SOURCE ${sources} APPEND PROPERTY OBJECT_DEPENDS ${input} ${pch_file})
    set_property(SOURCE ${sources} APPEND PROPERTY COMPILE_OPTIONS ${pch_flags})
endfunction()

function(add_precompiled_header target input)
    if(NOT enablePrecompiledHeaders)
        return()
    endif()

    if(${CMAKE_VERSION} VERSION_GREATER_EQUAL "3.16.0")
        target_precompile_headers(${target} PRIVATE ${input})
    elseif(MSVC)
        if(${CMAKE_GENERATOR} STREQUAL "Ninja")
            message(FATAL_ERROR "Precompiled header is not supported when using Ninja. Please update CMake version to 3.16 or use -DenablePrecompiledHeaders=OFF")
        endif()
        _add_msvc_precompiled_header(${target} ${input})
    elseif(XCODE)
        _add_xcode_precompiled_header(${target} ${input})
    elseif(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        _add_gcc_clang_precompiled_header(${target} ${input})
    else()
        message(FATAL_ERROR "Precompiled header is not supported for target ${target}")
    endif()
endfunction()
