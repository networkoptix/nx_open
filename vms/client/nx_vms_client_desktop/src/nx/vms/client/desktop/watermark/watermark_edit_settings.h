// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

class QWidget;
struct QnWatermarkSettings;

namespace nx::vms::client::desktop {

/** Returns true if settings were changed. */
bool editWatermarkSettings(QnWatermarkSettings& settings, QWidget* parent);

} // namespace nx::vms::client::desktop
