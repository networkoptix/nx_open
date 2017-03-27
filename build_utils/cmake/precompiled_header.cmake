function(_generate_pch_parameters target pch_dir)
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

    file(GENERATE OUTPUT "${pch_dir}.parameters" CONTENT
        "${flags}${include_directories}${framework_directories}${definitions}\n")
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
    set(_cxx_path "${CMAKE_CFG_INTDIR}/${_target}_cxx_pch")
    set(_c_path "${CMAKE_CFG_INTDIR}/${_target}_c_pch")
    make_directory("${_cxx_path}")
    make_directory("${_c_path}")
    set(_pch_cxx_header "${_cxx_path}/${_input}")
    set(_pch_cxx_pch "${_cxx_path}/${_input_we}.pch")
    set(_pch_c_header "${_c_path}/${_input}")
    set(_pch_c_pch "${_c_path}/${_input_we}.pch")

    get_target_property(sources ${_target} SOURCES)
    foreach(_source ${sources})
        set(_pch_compile_flags "")
        if(_source MATCHES \\.\(cc|cxx|cpp|c\)$)
            if(_source MATCHES \\.\(cpp|cxx|cc\)$)
                set(_pch_header "${_input}")
                set(_pch "${_pch_cxx_pch}")
            else()
                set(_pch_header "${_input}")
                set(_pch "${_pch_c_pch}")
        endif()

        if(_source STREQUAL "${_PCH_SOURCE_CXX}")
            set(_pch_compile_flags "${_pch_compile_flags} \"/Fp${_pch_cxx_pch}\" /Yc${_input}")
            set(_pch_source_cxx_found TRUE)
            set_source_files_properties("${_source}" PROPERTIES OBJECT_OUTPUTS "${_pch_cxx_pch}")
        elseif(_source STREQUAL "${_PCH_SOURCE_C}")
            set(_pch_compile_flags "${_pch_compile_flags} \"/Fp${_pch_c_pch}\" /Yc${_input}")
            set(_pch_source_c_found TRUE)
            set_source_files_properties("${_source}" PROPERTIES OBJECT_OUTPUTS "${_pch_c_pch}")
        else()
            if(_source MATCHES \\.\(cpp|cxx|cc\)$)
                set(_pch_compile_flags "${_pch_compile_flags} \"/Fp${_pch_cxx_pch}\" /Yu${_input}")
                set(_pch_source_cxx_needed TRUE)
                set_source_files_properties("${_source}" PROPERTIES OBJECT_DEPENDS "${_pch_cxx_pch}")
            else()
                set(_pch_compile_flags "${_pch_compile_flags} \"/Fp${_pch_c_pch}\" /Yu${_input}")
                set(_pch_source_c_needed TRUE)
                set_source_files_properties("${_source}" PROPERTIES OBJECT_DEPENDS "${_pch_c_pch}")
            endif()
            set(_pch_compile_flags "${_pch_compile_flags} /FI${_input}")
        endif()

        get_source_file_property(_object_depends "${_source}" OBJECT_DEPENDS)
        if(NOT _object_depends)
            set(_object_depends)
        endif()
        if(_source MATCHES \\.\(cc|cxx|cpp\)$)
            list(APPEND _object_depends "${_pch_header}")
        else()
            list(APPEND _object_depends "${_pch_header}")
        endif()

        set_source_files_properties(${_source} PROPERTIES
            COMPILE_FLAGS "${_pch_compile_flags}"
            OBJECT_DEPENDS "${_object_depends}")
        endif()
    endforeach()

    if(_pch_source_cxx_needed AND NOT _pch_source_cxx_found)
        message(FATAL_ERROR "A source file ${_PCH_SOURCE_CXX} for ${_input} is required for MSVC builds. Can be set with the SOURCE_CXX option.")
    endif()
    if(_pch_source_c_needed AND NOT _pch_source_c_found)
        message(FATAL_ERROR "A source file ${_PCH_SOURCE_C} for ${_input} is required for MSVC builds. Can be set with the SOURCE_C option.")
    endif()
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

    _generate_pch_parameters(${target} ${pch_dir})
    _get_cxx_standard(${target} cxx_standard)

    file(MAKE_DIRECTORY "${pch_dir}")
    add_custom_command(
        OUTPUT "${pch_file}"
        COMMAND "${CMAKE_CXX_COMPILER}"
            "@${pch_dir}.parameters" ${cxx_standard} -x c++-header "${input}" -o "${pch_file}"
        DEPENDS "${input}" "${pch_dir}.parameters"
        IMPLICIT_DEPENDS CXX "${input}"
        COMMENT "Precompiling ${pch_dir}")

    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        set(pch_flags "-Xclang -include-pch -Xclang \"${pch_file}\" -Winvalid-pch")
    else()
        set(pch_flags "-include \"${CMAKE_CURRENT_BINARY_DIR}/${target}\" -Winvalid-pch")
    endif()

    get_target_property(sources ${target} SOURCES)
    foreach(source ${sources})
        if(NOT source MATCHES "\\.\(cpp|cxx|cc\)$")
            continue()
        endif()

        get_source_file_property(flags "${source}" COMPILE_FLAGS)
        if(NOT flags)
            set(flags)
        endif()
        list(APPEND flags ${pch_flags})

        get_source_file_property(depends "${source}" OBJECT_DEPENDS)
        if(NOT depends)
            set(depends)
        endif()
        list(APPEND depends "${input}" "${pch_file}")

        set_source_files_properties("${source}"
            PROPERTIES
                COMPILE_FLAGS "${flags}"
                OBJECT_DEPENDS "${depends}")
    endforeach()
endfunction()

function(add_precompiled_header target input)
    if(MSVC)
        _add_msvc_precompiled_header(${target} ${input})
    elseif(XCODE)
        _add_xcode_precompiled_header(${target} ${input})
    elseif(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        _add_gcc_clang_precompiled_header(${target} ${input})
    else()
        message(FATAL_ERROR "Precompiled header is not supported for target ${target}")
    endif()
endfunction()
