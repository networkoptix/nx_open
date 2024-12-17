// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtGui/QColor>

#include <nx/vms/common/system_health/message_type.h>
#include <nx/vms/event/level.h>

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
Value valueOf(
    nx::vms::common::SystemContext* systemContext,
    nx::vms::common::system_health::MessageType messageType);
int priority(nx::vms::common::SystemContext* systemContext,
    nx::vms::common::system_health::MessageType messageType);

QColor notificationTextColor(Value level);
QColor notificationColor(Value level);

} // namespace QnNotificationLevel
