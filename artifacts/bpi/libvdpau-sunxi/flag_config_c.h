/*
This is a C wrapper for C++ module "flag_config".

NOTE: Compile with -DNX_UTILS_API='__attribute__ ((visibility ("hidden")))'

In your C program, add the following files:

conf.cpp:

#include "conf.h"

conf.h:

#pragma once

#define CONFIG_MODULE_NAME "mymodule"
#define CONFIG_BODY \
{ \
     NX_FLAG(0, myFlag, "Description"); \
     NX_INT_PARAM(7, myInt, "Description"); \
     NX_STRING_PARAM("default value", myStr, "Description"); \
}

#include "flag_config_c.h"

*/

#include <stdbool.h>

#define NX_FLAG(DEFAULT, NAME, DESCR) bool NAME
#define NX_INT_PARAM(DEFAULT, NAME, DESCR) int NAME
#define NX_STRING_PARAM(DEFAULT, NAME, DESCR) const char* NAME
struct Conf CONFIG_BODY;
#undef NX_FLAG
#undef NX_INT_PARAM
#undef NX_STRING_PARAM

#if defined(__cplusplus) //< Included from conf.cpp

    #include "flag_config.h"

    extern "C" {
        NX_UTILS_API Conf conf;
    } // extern "C"

    #undef NX_FLAG
    #define NX_FLAG(DEFAULT, NAME, DESCR) regFlagParam(&pConf->NAME, (DEFAULT), #NAME, (DESCR))
    #undef NX_INT_PARAM
    #define NX_INT_PARAM(DEFAULT, NAME, DESCR) regIntParam(&pConf->NAME, (DEFAULT), #NAME, (DESCR))
    #undef NX_STRING_PARAM
    #define NX_STRING_PARAM(DEFAULT, NAME, DESCR) regStringParam(&pConf->NAME, (DEFAULT), #NAME, (DESCR))

    namespace {

        class CppFlagConfig: public nx::utils::FlagConfig
        {
        public:
            Conf* pConf = &conf;

            CppFlagConfig(): nx::utils::FlagConfig(CONFIG_MODULE_NAME)
            CONFIG_BODY
        };
        CppFlagConfig cppConf;

    } // namespace

    extern "C" {
        NX_UTILS_API void conf_reload() { cppConf.reload(); }
        NX_UTILS_API void conf_skipNextReload() { cppConf.skipNextReload(); }
        NX_UTILS_API const char* conf_tempPath() { return cppConf.tempPath(); }
        NX_UTILS_API const char* conf_moduleName() { return cppConf.moduleName(); }
    } // extern "C"

#else // __cplusplus //< Included in C project

    extern NX_UTILS_API struct Conf conf;

    NX_UTILS_API void conf_reload();
    NX_UTILS_API void conf_skipNextReload();
    NX_UTILS_API const char* conf_tempPath();
    NX_UTILS_API const char* conf_moduleName();

#endif // __cplusplus
