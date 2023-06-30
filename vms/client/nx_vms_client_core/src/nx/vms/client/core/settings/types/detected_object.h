// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtGui/QColor>

#include <unordered_map>

#include <nx/reflect/instrument.h>
#include <nx/vms/common/serialization/qt_gui_types.h>

namespace nx::vms::client::core {

struct DetectedObjectSettings
{
    QColor color;

    bool operator==(const DetectedObjectSettings&) const = default;
};
NX_REFLECTION_INSTRUMENT(DetectedObjectSettings, (color))

using DetectedObjectSettingsMap = std::unordered_map<QString, DetectedObjectSettings>;

} // namespace nx::vms::client::core
