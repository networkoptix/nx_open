#ifndef QN_NOTIFICATION_LEVELS_H
#define QN_NOTIFICATION_LEVELS_H

#include <QtGui/QColor>

#include <business/business_fwd.h>
#include <client/client_globals.h>
#include <health/system_health.h>

class QnNotificationLevels {
public:
    static Qn::NotificationLevel notificationLevel(QnBusiness::EventType eventType);
    static Qn::NotificationLevel notificationLevel(QnSystemHealth::MessageType messageType);


    static QColor notificationColor(Qn::NotificationLevel level);
    static QColor notificationColor(QnBusiness::EventType eventType) {
        return notificationColor(notificationLevel(eventType));
    }
    static QColor notificationColor(QnSystemHealth::MessageType messageType) {
        return notificationColor(notificationLevel(messageType));
    }
};


#endif // QN_NOTIFICATION_LEVELS_H
