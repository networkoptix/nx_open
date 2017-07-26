#pragma once

#include <nx/kit/ini_config.h>

struct Ini: public nx::kit::IniConfig
{
    Ini(): IniConfig("mediaserver.ini") { reload(); }

    NX_INI_FLAG(0, verboseAutoRequestForwarder, "Set log level to Verbose for AutoRequestForwarder.");
    NX_INI_FLAG(0, ignoreApiModuleInformationInAutoRequestForwarder, "");
    NX_INI_FLAG(0, isP2pMode, "Switch messageBus to the new P2p implementation.");
};

inline Ini& ini()
{
    static Ini ini;
    return ini;
}
