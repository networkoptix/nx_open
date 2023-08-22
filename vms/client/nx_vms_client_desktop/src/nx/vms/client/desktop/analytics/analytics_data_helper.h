// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/client/desktop/resource_properties/camera/data/analytics_engine_info.h>
#include <nx/vms/common/resource/analytics_engine_resource.h>

namespace nx::vms::client::desktop {

enum class SettingsModelSource
{
    /* The model is made by the plugin developer */
    manifest,
    /* Current settings model, built from the manifest model, but is not required to be the same. */
    resourceProperty
};

AnalyticsEngineInfo engineInfoFromResource(
    const common::AnalyticsEngineResourcePtr& engine, SettingsModelSource settingsModelSource);

} // namespace nx::vms::client::desktop
