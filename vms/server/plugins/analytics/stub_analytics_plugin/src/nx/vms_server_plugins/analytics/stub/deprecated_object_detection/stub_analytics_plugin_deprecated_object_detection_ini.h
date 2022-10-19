// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/kit/ini_config.h>
#include <nx/sdk/analytics/helpers/pixel_format.h>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {
namespace deprecated_object_detection {

struct Ini: public nx::kit::IniConfig
{
    Ini(): IniConfig("stub_analytics_plugin_deprecated_object_detection.ini") { reload(); }

    NX_INI_FLAG(0, enableOutput, "");

    NX_INI_FLAG(0, deviceDependent, "Respective capability in the manifest.");

    NX_INI_FLAG(0, keepObjectBoundingBoxRotation,
        "If set, Engine will declare the corresponding capability in the manifest.");

    NX_INI_FLAG(1, declareStubObjectTypes,
        "If set, own Stub Analytics Plugin Object types (nx.stub.*) are declared in the\n"
        "typeLibrary and as supported ones. The corresponding section in the settings is also\n"
        "present.");
};

Ini& ini();

} // namespace deprecated_object_detection
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
