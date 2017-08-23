#pragma once

#include <nx/kit/ini_config.h>

namespace nx {
namespace client {
namespace desktop {

struct Ini: public nx::kit::IniConfig
{
    Ini(): IniConfig("desktop_client.ini") {}

    NX_INI_FLAG(0, enableEntropixEnchancer, "Enable Entropix image enchancement controls.");
};

inline Ini& ini()
{
    static Ini ini;
    return ini;
}

} // namespace desktop
} // namespace client
} // namespace nx
