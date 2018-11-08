#pragma once

#include <nx/kit/ini_config.h>

namespace nx {
namespace gstreamer {

struct NxGstreamerConfig: public nx::kit::IniConfig
{
    NxGstreamerConfig():
        nx::kit::IniConfig("nx_gstreamer.ini")
    {
        reload();
    }

    NX_INI_FLAG(1, enableOutput, "Enable debug output");
};

inline NxGstreamerConfig& ini()
{
    static NxGstreamerConfig nx_gstreamer_ini;
    return nx_gstreamer_ini;
}

} // namespace gstreamer
} // namespace nx
