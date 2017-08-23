#pragma once

#include <nx/kit/ini_config.h>

namespace nx {
namespace client {
namespace desktop {

struct Ini: public nx::kit::IniConfig
{
    Ini(): IniConfig("desktop_client.ini") {}

    NX_INI_FLAG(0, enableEntropixEnhancer, "Enable Entropix image enhancement controls.");
    NX_INI_STRING("http://96.64.226.250:8888/image",
        entropixEnhancerUrl, "URL of Entropix image enhancement API.");
};

inline Ini& ini()
{
    static Ini ini;
    return ini;
}

} // namespace desktop
} // namespace client
} // namespace nx
