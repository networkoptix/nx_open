// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

class QWidget;
namespace nx::vms::api { struct WatermarkSettings; }

namespace nx::vms::client::desktop {

/** Returns true if settings were changed. */
bool editWatermarkSettings(nx::vms::api::WatermarkSettings& settings, QWidget* parent);

} // namespace nx::vms::client::desktop
