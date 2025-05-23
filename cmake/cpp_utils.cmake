## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

function(nx_get_target_cpp_sources target variable)
    get_target_property(sources ${target} SOURCES)
    set(result)
    foreach(file ${sources})
        if(file MATCHES "\\.\(cpp|cxx|cc\)$")
            list(APPEND result ${file})
        endif()
    endforeach()

    set(${variable} ${result} PARENT_SCOPE)
endfunction()

function(nx_force_include target)
    set(flags)
    foreach(file ${ARGN})
        if(MSVC)
            list(APPEND flags /FI${file})
        else()
            list(APPEND flags -include ${file})
        endif()
    endforeach()

    if(CMAKE_GENERATOR MATCHES "Visual Studio")
        target_compile_options(${target} PRIVATE ${flags})
    else()
        # target_compile_options cannot be used! Now source file property is the only right way to
        # do this. This is because of precompiled headers. GCC require its inclusion to be before
        # any other `-include` flags. To guarantee this, we must call this function after PCH
        # activation and this function must set flags in the same way that PCH is activated
        # (source file property).
        nx_get_target_cpp_sources(${target} sources)
        set_property(SOURCE ${sources} APPEND PROPERTY COMPILE_OPTIONS ${flags})
    endif()
endfunction()

# Wrap interface compile options of a C++ target with -Xcompiler= (for CUDA compiler).
function(nx_fix_target_interface_for_cuda target)
    if(NOT "${CMAKE_CUDA_COMPILER_ID}" STREQUAL "Clang")
        get_property(old_flags TARGET ${target} PROPERTY INTERFACE_COMPILE_OPTIONS)
        if(NOT "${old_flags}" STREQUAL "")
            string(REPLACE ";" "," CUDA_flags "${old_flags}")
            set_property(TARGET ${target} PROPERTY INTERFACE_COMPILE_OPTIONS
                "$<$<BUILD_INTERFACE:$<COMPILE_LANGUAGE:CXX>>:${old_flags}>$<$<BUILD_INTERFACE:$<COMPILE_LANGUAGE:CUDA>>:-Xcompiler=${CUDA_flags}>"
                )
        endif()
    endif()
endfunction()
