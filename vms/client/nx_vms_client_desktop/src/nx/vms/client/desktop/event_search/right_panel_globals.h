// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QMetaType>

#include <nx/vms/api/types/event_rule_types.h>

namespace nx::vms::client::desktop {
namespace RightPanel {

Q_NAMESPACE
Q_CLASSINFO("RegisterEnumClassesUnscoped", "false")

enum class Tab
{
    invalid = -1,
    resources,
    motion,
    bookmarks,
    events,
    analytics,
    settings
};
Q_ENUM_NS(Tab)

enum class FetchDirection
{
    earlier,
    later
};
Q_ENUM_NS(FetchDirection)

enum class FetchResult
{
    complete, //< Successful. There's no more data to fetch.
    incomplete, //< Successful. There's more data to fetch.
    failed, //< Unsuccessful.
    cancelled //< Cancelled.
};
Q_ENUM_NS(FetchResult)

enum class CameraSelection
{
    all,
    layout,
    current,
    custom
};
Q_ENUM_NS(CameraSelection)

enum class SystemSelection
{
    all,
    current,
};
Q_ENUM_NS(SystemSelection)

enum class TimeSelection
{
    anytime,
    day,
    week,
    month,
    selection
};
Q_ENUM_NS(TimeSelection)

enum class PreviewState
{
    initial,
    busy,
    ready,
    missing
};
Q_ENUM_NS(PreviewState)

struct EventCategory
{
    enum Type
    {
        Any = nx::vms::api::EventType::undefinedEvent,
        Analytics = nx::vms::api::EventType::analyticsSdkEvent,
        Generic = nx::vms::api::EventType::userDefinedEvent,
        Input = nx::vms::api::EventType::cameraInputEvent,
        SoftTrigger = nx::vms::api::EventType::softwareTriggerEvent,
        StreamIssue = nx::vms::api::EventType::networkIssueEvent,
        DeviceDisconnect = nx::vms::api::EventType::cameraDisconnectEvent,
        DeviceIpConflict = nx::vms::api::EventType::cameraIpConflictEvent
    };
    Q_ENUM(Type)

    QString name;
    QString icon;
    Type type;

    Q_GADGET
    Q_PROPERTY(QString name MEMBER name CONSTANT)
    Q_PROPERTY(QString icon MEMBER icon CONSTANT)
    Q_PROPERTY(Type type MEMBER type CONSTANT)
};

void registerQmlType();

inline uint qHash(RightPanel::TimeSelection source)
{
    return uint(source);
}

inline uint qHash(RightPanel::CameraSelection source)
{
    return uint(source);
}

inline uint qHash(RightPanel::SystemSelection source)
{
    return uint(source);
}

} // namespace RightPanel
} // namespace nx::vms::client::desktop

Q_DECLARE_METATYPE(nx::vms::client::desktop::RightPanel::EventCategory)
