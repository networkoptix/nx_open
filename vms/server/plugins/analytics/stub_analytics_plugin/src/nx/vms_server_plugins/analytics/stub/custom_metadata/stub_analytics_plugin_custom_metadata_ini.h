// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/kit/ini_config.h>
#include <nx/sdk/analytics/helpers/pixel_format.h>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {
namespace custom_metadata {

struct Ini: public nx::kit::IniConfig
{
    Ini(): IniConfig("stub_analytics_plugin_custom_metadata.ini") { reload(); }

    NX_INI_FLAG(0, enableOutput, "");

    NX_INI_FLAG(0, deviceDependent, "Respective capability in the manifest.");

    NX_INI_FLAG(1, needMetadata,
        "If set, Engine will declare the corresponding stream type filter in the manifest.");
};

Ini& ini();

} // namespace custom_metadata
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
