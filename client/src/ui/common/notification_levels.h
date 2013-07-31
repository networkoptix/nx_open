#ifndef QN_NOTIFICATION_LEVELS_H
#define QN_NOTIFICATION_LEVELS_H

#include <business/business_fwd.h>

#include <client/client_globals.h>

class QnNotificationLevels {
public:
    static Qn::NotificationLevel notificationLevel(BusinessEventType::Value eventType);
    
    static QColor notificationColor(Qn::NotificationLevel level);

    static QColor notificationColor(BusinessEventType::Value eventType) {
        return notificationColor(notificationLevel(eventType));
    }
};


#endif // QN_NOTIFICATION_LEVELS_H
