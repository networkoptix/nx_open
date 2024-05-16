// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QMetaType>
#include <QtCore/QVector>

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

enum class SystemSelection
{
    organization,
    current,
};
Q_ENUM_NS(SystemSelection)

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

} // namespace RightPanel
} // namespace nx::vms::client::desktop
