// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/kit/ini_config.h>
#include <nx/sdk/analytics/helpers/pixel_format.h>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {
namespace special_objects {

struct Ini: public nx::kit::IniConfig
{
    Ini(): IniConfig("stub_analytics_plugin_special_objects.ini") { reload(); }

    NX_INI_FLAG(0, enableOutput, "");

    NX_INI_FLAG(0, deviceDependent, "Respective capability in the manifest.");

    NX_INI_STRING("http://internal.server/addPerson?trackId=", addPersonActionUrlPrefix,
        "Prefix for the URL returned by addPerson action; track id will be appended to this "
        "prefix.");

    NX_INI_FLAG(0, keepObjectBoundingBoxRotation,
        "If set, Engine will declare the corresponding capability in the manifest.");
};

Ini& ini();

} // namespace special_objects
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
