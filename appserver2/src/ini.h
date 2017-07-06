#pragma once

#include <nx/kit/ini_config.h>

namespace ec2 {

struct Ini: public nx::kit::IniConfig
{
    Ini(): IniConfig("appserver2.ini") { reload(); }

    NX_INI_FLAG(0, isP2pMode, "Switch messageBus to the new P2p implementation.");
};

inline Ini& ini()
{
    static Ini ini;
    return ini;
}

} // namespace ec2
