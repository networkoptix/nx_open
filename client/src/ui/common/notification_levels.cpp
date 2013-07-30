#include "notification_levels.h"

#include <utils/common/warnings.h>

#include <ui/style/globals.h>

Qn::NotificationLevel QnNotificationLevels::notificationLevel(BusinessEventType::Value eventType) {
    switch(eventType) {
    case BusinessEventType::Camera_Motion:          return Qn::CommonNotification;
    case BusinessEventType::Camera_Input:           return Qn::CommonNotification;      
    case BusinessEventType::Camera_Disconnect:      return Qn::ImportantNotification;
    case BusinessEventType::Storage_Failure:        return Qn::ImportantNotification;
    case BusinessEventType::Network_Issue:          return Qn::ImportantNotification;
    case BusinessEventType::Camera_Ip_Conflict:     return Qn::CriticalNotification;
    case BusinessEventType::MediaServer_Failure:    return Qn::CriticalNotification;
    case BusinessEventType::MediaServer_Conflict:   return Qn::CriticalNotification;
    case BusinessEventType::SystemHealthMessage:    return Qn::SystemNotification;
    default:                                        
        qnWarning("Invalid business event type '%1'.", static_cast<int>(eventType));
        return Qn::NoNotification;
    }
}

QColor QnNotificationLevels::notificationColor(Qn::NotificationLevel level) {
    switch (level) {
    case Qn::NoNotification:        return Qt::transparent;
    case Qn::OtherNotification:     return Qt::white;
    case Qn::CommonNotification:    return qnGlobals->notificationColorCommon();
    case Qn::ImportantNotification: return qnGlobals->notificationColorImportant();
    case Qn::CriticalNotification:  return qnGlobals->notificationColorCritical();
    case Qn::SystemNotification:    return qnGlobals->notificationColorSystem();
    default:
        qnWarning("Invalid notification level '%1'.", static_cast<int>(level));
        return QColor();
    }
}
