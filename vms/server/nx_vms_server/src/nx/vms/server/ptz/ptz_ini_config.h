#pragma once

#include <nx/kit/ini_config.h>

#include <core/ptz/ptz_constants.h>

namespace nx {
namespace vms::server {
namespace ptz {

struct PtzIniConfig: public nx::kit::IniConfig
{
    PtzIniConfig(): IniConfig("server_ptz.ini") {}

    NX_INI_STRING(
        "",
        capabilitiesToRemove,
        "PTZ capabilities that will be removed from every camera.");

    NX_INI_STRING(
        "",
        capabilitiesToAdd,
        "PTZ capabilities that will be added to every camera.");

    Ptz::Capabilities addedCapabilities() const;

    Ptz::Capabilities excludedCapabilities() const;
};

inline PtzIniConfig& ini()
{
    static PtzIniConfig ini;
    return ini;
}

} // namespace ptz
} // namespace vms::server
} // namespace nx
