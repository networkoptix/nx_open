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

    if(CMAKE_GENERATOR MATCHES "Visual Studio" AND enablePrecompiledHeaders)
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
