#pragma once

#include <nx/kit/ini_config.h>

namespace nx::vms::client::desktop::integrations {

struct Config: nx::kit::IniConfig
{
    Config(): IniConfig("desktop_client_integrations.ini") { reload(); }

    NX_INI_STRING("", enableEntropixZoomWindowReconstructionOn,
        "Enable Entropix 'Reconstruct Resolution' integration on the cameras, which model matches "
        "the provided string, counted as a regular expression.");

    NX_INI_FLAG(0, enableRoiVisualization, "Enable ROI visualization over the video.");
};

inline Config& config()
{
    static Config Config;
    return Config;
}

} // namespace nx::vms::client::desktop::integrations
