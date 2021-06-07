// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

/**@file
 * C99 wrapper for C++ module "ini_config". Only one .ini instance is supported. Can be included in
 * both C++ and C99 source files.
 *
 * If compiled into a library, define -DNX_KIT_C_API as needed for import/export directives.
 *
 * In your C99 program, use .ini file as follows:
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
 * In your C99 project, add nx/kit/ini_config.cpp and the following files:
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
 *     #include <ini_config_c.h>
 *
 * </code></pre>
 *
 * ini.cpp:
 * <pre><code>
 *
 *     #include "ini.h"
 *
 *     #include <ini_config_c_impl.h>
 *
 * </code></pre>
 */

#if !defined(__cplusplus)
    #include <stdbool.h>
#endif // !defined(__cplusplus)

#if defined(__cplusplus)
    extern "C" {
#endif

#define NX_INI_FLAG(DEFAULT, PARAM, DESCR) bool PARAM
#define NX_INI_INT(DEFAULT, PARAM, DESCR) int PARAM
#define NX_INI_STRING(DEFAULT, PARAM, DESCR) const char* PARAM
#define NX_INI_FLOAT(DEFAULT, PARAM, DESCR) float PARAM

struct Ini NX_INI_STRUCT; //< Ini struct definition: expands using the macros defined above.

#undef NX_INI_FLAG
#undef NX_INI_INT
#undef NX_INI_STRING
#undef NX_INI_FLOAT

enum NxIniOutput { NX_INI_OUTPUT_NONE, NX_INI_OUTPUT_STDOUT, NX_INI_OUTPUT_STDERR };

#if !defined(NX_KIT_C_API)
    #define NX_KIT_C_API
#endif

NX_KIT_C_API extern struct Ini ini; //< Declaration of Ini instance global variable.

// See the documentation of the respective methods of class IniConfig in "ini_config.h".
NX_KIT_C_API bool nx_ini_isEnabled(void);
NX_KIT_C_API void nx_ini_setOutput(enum NxIniOutput output);
NX_KIT_C_API void nx_ini_reload(void);
NX_KIT_C_API const char* nx_ini_iniFile(void);
NX_KIT_C_API void nx_ini_setIniFilesDir(const char* value);
NX_KIT_C_API const char* nx_ini_iniFilesDir(void);
NX_KIT_C_API const char* nx_ini_iniFilePath(void);

#if defined(__cplusplus)
    } // extern "C"
#endif
