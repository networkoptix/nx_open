/**@file
 * This is a C99 wrapper for C++ module "ini_config". Only one .ini instance is supported for now.
 *
 * NOTE: Compile with -DNX_KIT_API='__attribute__((visibility ("hidden")))'
 *
 * In your C program, use .ini options as follows:
 * <pre><code>
 *
 *     #include "ini.h"
 *     ...
 *     int main()
 *     {
 *         nx_ini_reload();
 *         ...
 *         if (ini.myFlag)
 *             ...
 *     }
 *
 * </code></pre>
 *
 * In your C project, add nx/kit/ini_config.cpp and the following files:
 *
 * ini.cpp:
 * <pre><code>
 *
 *     #include "ini.h"
 *
 * </code></pre>
 *
 * ini.h:
 * <pre><code>
 *
 *     #pragma once
 *
 *     #define NX_INI_FILE "my_module.ini"
 *     #define NX_INI_STRUCT \
 *     { \
 *         NX_INI_FLAG(0, myFlag, "Description"); \
 *         NX_INI_INT(7, myInt, "Description"); \
 *         NX_INI_STRING("default value", myStr, "Description"); \
 *     }
 *
 *     #include "ini_config_c.h"
 *
 * </code></pre>
 */

#include <stdbool.h>

#define NX_INI_FLAG(DEFAULT, PARAM, DESCR) bool PARAM
#define NX_INI_INT(DEFAULT, PARAM, DESCR) int PARAM
#define NX_INI_STRING(DEFAULT, PARAM, DESCR) const char* PARAM

struct Ini NX_INI_STRUCT;

#undef NX_INI_FLAG
#undef NX_INI_INT
#undef NX_INI_STRING

enum NxIniOutput { NX_INI_OUTPUT_NONE, NX_INI_OUTPUT_STDOUT, NX_INI_OUTPUT_STDERR };

//-------------------------------------------------------------------------------------------------
#if !defined(__cplusplus) // Included in C project

NX_KIT_API extern const struct Ini ini; //< Global variable declaration. Definition is in ini.cpp.

NX_KIT_API bool nx_ini_isEnabled(void);
NX_KIT_API void nx_ini_setOutput(enum NxIniOutput output);
NX_KIT_API void nx_ini_reload(void);
NX_KIT_API const char* nx_ini_iniFile(void);
NX_KIT_API const char* nx_ini_iniFileDir(void);
NX_KIT_API const char* nx_ini_iniFilePath(void);

//-------------------------------------------------------------------------------------------------
#else // !defined(__cplusplus) // Included from ini.cpp

#include <nx/kit/ini_config.h>

using nx::kit::IniConfig;

extern "C" {
    NX_KIT_API struct Ini ini; //< Global variable definition. Non-const to assign default values.
} // extern "C"

namespace {

struct CppIni: public IniConfig
{
    CppIni(): IniConfig(NX_INI_FILE)
    {
        // Call reg...Param() for each param, and assign default values.

        #undef NX_INI_FLAG
        #define NX_INI_FLAG(DEFAULT, PARAM, DESCR) \
            pIni->PARAM = regBoolParam(&pIni->PARAM, (DEFAULT), #PARAM, (DESCR))

        #undef NX_INI_INT
        #define NX_INI_INT(DEFAULT, PARAM, DESCR) \
            pIni->PARAM = regIntParam(&pIni->PARAM, (DEFAULT), #PARAM, (DESCR))

        #undef NX_INI_STRING
        #define NX_INI_STRING(DEFAULT, PARAM, DESCR) \
            pIni->PARAM = regStringParam(&pIni->PARAM, (DEFAULT), #PARAM, (DESCR))

        NX_INI_STRUCT //< Expands using the macros defined above.
    }

    Ini* pIni = &ini;
};

static CppIni cppIni;

} // namespace

extern "C" {

NX_KIT_API bool nx_ini_isEnabled() { return IniConfig::isEnabled(); }

NX_KIT_API void nx_ini_setOutput(enum NxIniOutput output)
{
    switch (output)
    {
        case NX_INI_OUTPUT_NONE: IniConfig::setOutput(nullptr); break;
        case NX_INI_OUTPUT_STDOUT: IniConfig::setOutput(&std::cout); break;
        case NX_INI_OUTPUT_STDERR: IniConfig::setOutput(&std::cerr); break;
        default:
            std::cerr << "nx_ini_setOutput(): INTERNAL ERROR: Invalid NxIniOutput: "
                << output << std::endl;
    }
}

NX_KIT_API void nx_ini_reload() { cppIni.reload(); }
NX_KIT_API const char* nx_ini_iniFile() { return cppIni.iniFile(); }
NX_KIT_API const char* nx_ini_iniFileDir() { return cppIni.iniFileDir(); }
NX_KIT_API const char* nx_ini_iniFilePath() { return cppIni.iniFilePath(); }

} // extern "C"

#endif // !defined(__cplusplus)
