#ifndef SYSTEM_HEALTH_BUSINESS_ACTION_H
#define SYSTEM_HEALTH_BUSINESS_ACTION_H

#include <business/actions/popup_business_action.h>
#include <health/system_health.h>

class QnSystemHealthBusinessAction : public QnPopupBusinessAction
{
    typedef QnPopupBusinessAction base_type;
public:
    QnSystemHealthBusinessAction(QnSystemHealth::MessageType message, int eventResourceId = 0);
};

typedef QSharedPointer<QnSystemHealthBusinessAction> QnSystemHealthBusinessActionPtr;

#endif // SYSTEM_HEALTH_BUSINESS_ACTION_H
