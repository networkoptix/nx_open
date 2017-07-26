#pragma once

#include <nx/kit/ini_config.h>

namespace nx {
namespace client {
namespace desktop {
namespace ui {
namespace action {

struct IniConfig: public nx::kit::IniConfig
{
    IniConfig(): nx::kit::IniConfig("client_analytics.ini") { reload(); }

    NX_INI_FLAG(0, enableAnalytics, "");
};

inline IniConfig& ini()
{
    static IniConfig ini;
    return ini;
}

} // namespace action
} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
