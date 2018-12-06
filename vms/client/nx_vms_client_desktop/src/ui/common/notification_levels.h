#pragma once

#include <QtCore/QMetaType>
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
    CommonNotification,
    OtherNotification,
    SuccessNotification,
    ImportantNotification,
    CriticalNotification,
    LevelCount
};

Value valueOf(const nx::vms::event::AbstractActionPtr& action);
Value valueOf(const nx::vms::event::EventParameters& params);
Value valueOf(QnSystemHealth::MessageType messageType);

QColor notificationTextColor(Value level);
QColor notificationColor(Value level);

};

Q_DECLARE_METATYPE(QnNotificationLevel::Value)
