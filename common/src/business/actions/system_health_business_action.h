#ifndef SYSTEM_HEALTH_BUSINESS_ACTION_H
#define SYSTEM_HEALTH_BUSINESS_ACTION_H

#include <business/actions/common_business_action.h>
#include <health/system_health.h>

class QnSystemHealthBusinessAction : public QnCommonBusinessAction
{
    typedef QnCommonBusinessAction base_type;
public:
    QnSystemHealthBusinessAction(QnSystemHealth::MessageType message, const QnUuid& eventResourceId = QnUuid());
};

typedef QSharedPointer<QnSystemHealthBusinessAction> QnSystemHealthBusinessActionPtr;

#endif // SYSTEM_HEALTH_BUSINESS_ACTION_H
