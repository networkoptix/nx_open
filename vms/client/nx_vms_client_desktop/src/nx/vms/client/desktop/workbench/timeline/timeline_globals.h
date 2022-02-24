// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QMetaType>

namespace nx::vms::client::desktop::workbench {
namespace timeline {

Q_NAMESPACE
Q_CLASSINFO("RegisterEnumClassesUnscoped", "false")

enum class TimeMarkerMode
{
    normal,
    leftmost,
    rightmost
};
Q_ENUM_NS(TimeMarkerMode)

enum class TimeMarkerDisplay
{
    full,
    compact,
    automatic
};
Q_ENUM_NS(TimeMarkerDisplay)

void registerQmlType();

} // namespace timeline
} // namespace nx::vms::client::desktop::workbench
