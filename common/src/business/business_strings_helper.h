#ifndef __BUSINESS_STRINGS_HELPER_H__
#define __BUSINESS_STRINGS_HELPER_H__

#include "actions/abstract_business_action.h"
#include "events/abstract_business_event.h"
#include "business_aggregation_info.h"
#include "business_logic_common.h"

class QnBusinessStringsHelper: QObject
{
public:
    static QString eventReason(const QnBusinessEventParameters& params);
    static QString longEventDescription(const QnAbstractBusinessAction* action, const QnBusinessAggregationInfo& aggregationInfo);

    static QString resourceUrl(const QnBusinessEventParameters &params);
    static QString resourceName(const QnBusinessEventParameters &params);

    static QString conflictString(const QnBusinessEventParameters &params, QLatin1Char delim = QLatin1Char('\n'));
    static QString motionUrl(const QnBusinessEventParameters &params);
    static QString eventTextString(BusinessEventType::Value eventType, const QnBusinessEventParameters &params);
private:
    static QString reasonString(const QnBusinessEventParameters &params);
    static QString timestampString(const QnBusinessEventParameters &params, int aggregationCount);
};

#endif // __BUSINESS_STRINGS_HELPER_H__
