// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QMetaType>
#include <QtGui/QColor>

#include <nx/vms/event/level.h>
#include <health/system_health.h>

namespace QnNotificationLevel {

enum class Value
{
    NoNotification = (int) nx::vms::event::Level::none,
    CommonNotification = (int) nx::vms::event::Level::common,
    OtherNotification = (int) nx::vms::event::Level::other,
    SuccessNotification = (int) nx::vms::event::Level::success,
    ImportantNotification = (int) nx::vms::event::Level::important,
    CriticalNotification = (int) nx::vms::event::Level::critical,
    LevelCount = (int) nx::vms::event::Level::count
};

Value convert(nx::vms::event::Level level);

Value valueOf(const nx::vms::event::AbstractActionPtr& action);
Value valueOf(const nx::vms::event::EventParameters& params);
Value valueOf(QnSystemHealth::MessageType messageType);

QColor notificationTextColor(Value level);
QColor notificationColor(Value level);

} // namespace QnNotificationLevel

Q_DECLARE_METATYPE(QnNotificationLevel::Value)
