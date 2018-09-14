#pragma once

#include <nx/kit/ini_config.h>

#include <ini_config_c.h> //< Needed for IDE convenience, since already included from "ini.h".

using nx::kit::IniConfig;

struct Ini ini; //< Definition of Ini instance global variable.

namespace {

struct CppIni: public IniConfig
{
    CppIni(): IniConfig(NX_INI_FILE)
    {
        // Call reg...Param() for each param, and assign default values.

        #undef NX_INI_FLAG
        #undef NX_INI_INT
        #undef NX_INI_STRING
        #undef NX_INI_FLOAT
        #undef NX_INI_DOUBLE

        #define NX_INI_FLAG(DEFAULT, PARAM, DESCR) \
            pIni->PARAM = regBoolParam(&pIni->PARAM, (DEFAULT), #PARAM, (DESCR))
        #define NX_INI_INT(DEFAULT, PARAM, DESCR) \
            pIni->PARAM = regIntParam(&pIni->PARAM, (DEFAULT), #PARAM, (DESCR))
        #define NX_INI_STRING(DEFAULT, PARAM, DESCR) \
            pIni->PARAM = regStringParam(&pIni->PARAM, (DEFAULT), #PARAM, (DESCR))
        #define NX_INI_FLOAT(DEFAULT, PARAM, DESCR) \
            pIni->PARAM = regFloatParam(&pIni->PARAM, (DEFAULT), #PARAM, (DESCR))
        #define NX_INI_DOUBLE(DEFAULT, PARAM, DESCR) \
            pIni->PARAM = regDoubleParam(&pIni->PARAM, (DEFAULT), #PARAM, (DESCR))

        NX_INI_STRUCT //< Fields initialization: expands using the macros defined above.
    }

    Ini* const pIni = &ini;
};

static CppIni cppIni;

} // namespace

bool nx_ini_isEnabled()
{
    return IniConfig::isEnabled();
}

void nx_ini_setOutput(NxIniOutput output)
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

void nx_ini_reload()
{
    cppIni.reload();
}

const char* nx_ini_iniFile()
{
    return cppIni.iniFile();
}

void nx_ini_setIniFilesDir(const char* value)
{
    IniConfig::setIniFilesDir(value);
}

const char* nx_ini_iniFilesDir()
{
    return IniConfig::iniFilesDir();
}

const char* nx_ini_iniFilePath()
{
    return cppIni.iniFilePath();
}
