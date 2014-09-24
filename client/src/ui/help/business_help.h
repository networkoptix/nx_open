#ifndef QN_BUSINESS_HELP_H
#define QN_BUSINESS_HELP_H

#include <business/business_fwd.h>
#include <health/system_health.h>

namespace QnBusiness {

    int eventHelpId(QnBusiness::EventType type);
    int actionHelpId(QnBusiness::ActionType type);
    int healthHelpId(QnSystemHealth::MessageType type);

} // namespace QnBusiness 

#endif // QN_BUSINESS_HELP_H
