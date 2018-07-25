# Test for disabled (via -DNX_INI_CONFIG_DISABLED) ini_config.
add_library(disabled_ini_config_ut SHARED
    ../src/nx/kit/ini_config.cpp
    ../src/nx/kit/test.cpp
    ini_config_ut.cpp
    disabled_ini_config_ut.cpp
)
target_compile_definitions(disabled_ini_config_ut
    PRIVATE NX_INI_CONFIG_DISABLED
    PRIVATE DISABLED_INI_CONFIG_UT_API=${NX_KIT_API_EXPORT_MACRO}
    INTERFACE DISABLED_INI_CONFIG_UT_API=${NX_KIT_API_IMPORT_MACRO}
    PRIVATE NX_KIT_API= #< nx_kit sources are statically linked into lib disabled_ini_config_ut.
)

# Executable for all tests.
add_executable(nx_kit_ut
    ini_config_ut.cpp
    debug_ut.cpp
    ini.cpp
    ini_config_c_ut.c
    main.cpp
)
target_link_libraries(nx_kit_ut
    nx_kit
    disabled_ini_config_ut
)
add_test(NAME nx_kit_ut COMMAND nx_kit_ut)
