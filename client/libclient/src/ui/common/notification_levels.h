#pragma once

#include <QtGui/QColor>

#include <business/business_fwd.h>
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

Value valueOf(const QnAbstractBusinessActionPtr &businessAction);
Value valueOf(QnSystemHealth::MessageType messageType);

QColor notificationColor(Value level);

};
