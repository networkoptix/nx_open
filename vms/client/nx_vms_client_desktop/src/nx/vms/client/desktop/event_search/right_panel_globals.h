// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QMetaType>
#include <QtCore/QVector>

#include <nx/vms/api/types/event_rule_types.h>

namespace nx::vms::client::desktop {
namespace RightPanel {

Q_NAMESPACE
Q_CLASSINFO("RegisterEnumClassesUnscoped", "false")

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

struct VmsEvent
{
    nx::vms::api::EventType id;
    QString name;

    Q_GADGET
    Q_PROPERTY(nx::vms::api::EventType id MEMBER id CONSTANT)
    Q_PROPERTY(QString name MEMBER name CONSTANT)
};

struct VmsEventGroup: public VmsEvent
{
    enum Type
    {
        Common,
        Server,
        DeviceIssues,
        Analytics
    };
    Q_ENUM(Type)

    Type type;
    QString any;
    QVector<VmsEvent> events;

    VmsEventGroup() = default;
    VmsEventGroup(Type type, nx::vms::api::EventType id, const QString& name, const QString& any);

    Q_GADGET
    Q_PROPERTY(Type type MEMBER type CONSTANT)
    Q_PROPERTY(QString any MEMBER any CONSTANT)
    Q_PROPERTY(QVector<VmsEvent> events MEMBER events CONSTANT)
};

void registerQmlType();

inline size_t qHash(RightPanel::TimeSelection source)
{
    return size_t(source);
}

inline size_t qHash(RightPanel::CameraSelection source)
{
    return size_t(source);
}

inline uint qHash(RightPanel::SystemSelection source)
{
    return uint(source);
}

} // namespace RightPanel
} // namespace nx::vms::client::desktop
