// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QMetaType>

namespace nx::vms::client::desktop {

enum class CameraSettingsTab
{
    general,
    recording,
    io,
    motion,
    dewarping,
    advanced,
    web,
    analytics,
    expert,
};

} // namespace nx::vms::client::desktop

Q_DECLARE_METATYPE(nx::vms::client::desktop::CameraSettingsTab)