#--------------------------------------------------------------------------------------------------
# Test for disabled (via -DNX_INI_CONFIG_DISABLED) ini_config.

add_library(disabled_ini_config_ut SHARED
    ../src/nx/kit/ini_config.h
    ../src/nx/kit/ini_config.cpp
    ../src/nx/kit/test.h
    ../src/nx/kit/test.cpp
    ../src/nx/kit/output_redirector.h
    ../src/nx/kit/output_redirector.cpp
    src/ini_config_ut.cpp
    src/disabled_ini_config_ut.h
    src/disabled_ini_config_ut.cpp
)

target_include_directories(disabled_ini_config_ut PRIVATE ../src)

if(WIN32)
    set(DISABLED_INI_CONFIG_UT_API_IMPORT_MACRO "__declspec(dllimport)")
    set(DISABLED_INI_CONFIG_UT_API_EXPORT_MACRO "__declspec(dllexport)")
else()
    set(DISABLED_INI_CONFIG_UT_API_IMPORT_MACRO "")
    set(DISABLED_INI_CONFIG_UT_API_EXPORT_MACRO "__attribute__((visibility(\"default\")))")
endif()

target_compile_definitions(disabled_ini_config_ut
    PRIVATE NX_INI_CONFIG_DISABLED
    PRIVATE DISABLED_INI_CONFIG_UT_API=${DISABLED_INI_CONFIG_UT_API_EXPORT_MACRO}
    INTERFACE DISABLED_INI_CONFIG_UT_API=${DISABLED_INI_CONFIG_UT_API_IMPORT_MACRO}
    PRIVATE NX_KIT_API= #< nx_kit sources are statically linked into lib disabled_ini_config_ut.
)

#--------------------------------------------------------------------------------------------------
# Executable for all tests.

add_executable(nx_kit_ut
    src/output_redirector_ut.cpp
    src/debug_ut.cpp
    src/ini_config_ut.cpp
    src/ini.h
    src/ini.cpp
    src/ini_config_c_usage.h
    src/ini_config_c_usage.c
    src/ini_config_c_ut.cpp
    src/test_ut.cpp
    src/main.cpp
)

target_link_libraries(nx_kit_ut
    nx_kit
    disabled_ini_config_ut
)

add_test(NAME nx_kit_ut COMMAND nx_kit_ut)
