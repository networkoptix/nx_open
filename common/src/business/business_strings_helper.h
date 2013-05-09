#ifndef __BUSINESS_STRINGS_HELPER_H__
#define __BUSINESS_STRINGS_HELPER_H__

#include "actions/abstract_business_action.h"
#include "events/abstract_business_event.h"
#include "business_aggregation_info.h"
#include "business_logic_common.h"

class QnBusinessStringsHelper: QObject
{
public:
    static QString eventReason(const QnAbstractBusinessAction* action);
    static QString longEventDescription(const QnAbstractBusinessAction* action, const QnBusinessAggregationInfo& aggregationInfo);

    static QString resourceUrl(const QnBusinessParams &params);
    static QString resourceName(const QnBusinessParams &params);

    static QString conflictString(const QnBusinessParams &params);
    static QString motionUrl(const QnBusinessParams &params);
private:
    static QString eventTextString(BusinessEventType::Value eventType, const QnBusinessParams &params);
    static QString reasonString(const QnBusinessParams &params);
    static QString timestampString(const QnBusinessParams &params, int aggregationCount);
};

#endif // __BUSINESS_STRINGS_HELPER_H__
