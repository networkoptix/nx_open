#pragma once

#include <QtGui/QColor>

#include <nx/vms/event/event_fwd.h>
#include <health/system_health.h>

namespace QnNotificationLevel {

/**
 * Importance level of a notification.
 */
enum class Value
{
    NoNotification,
    OtherNotification,
    CommonNotification,
    ImportantNotification,
    CriticalNotification,
    LevelCount
};

Value valueOf(const nx::vms::event::AbstractActionPtr &businessAction);
Value valueOf(QnSystemHealth::MessageType messageType);

QColor notificationColor(Value level);

};
