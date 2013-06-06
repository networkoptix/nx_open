#ifndef __BUSINESS_STRINGS_HELPER_H__
#define __BUSINESS_STRINGS_HELPER_H__

#include "actions/abstract_business_action.h"
#include "events/abstract_business_event.h"
#include "business_aggregation_info.h"
#include "business_fwd.h"

class QnBusinessStringsHelper: QObject
{
public:
    static QString eventReason(const QnBusinessEventParameters& params);

    /** Name of the event in common, e.g. 'Motion on Camera' */
    static QString eventName(BusinessEventType::Value value);

    /** Short description of the event */
    static QString shortEventDescription(const QnBusinessEventParameters &params);

    /** Full event description */
    static QString longEventDescription(const QnAbstractBusinessActionPtr& action,
                                            const QnBusinessAggregationInfo& aggregationInfo = QnBusinessAggregationInfo());

    /** Full description in HTML format */
    static QString longEventDescriptionHtml(const QnAbstractBusinessActionPtr& action,
                                            const QnBusinessAggregationInfo& aggregationInfo = QnBusinessAggregationInfo());

    /**
    * Short description. Contain event params only. Doesn't include event type to message
    */
    static QString eventParamsString(BusinessEventType::Value eventType, const QnBusinessEventParameters &params);

    static QString motionUrl(const QnBusinessEventParameters &params);
    static QString formatEmailList(const QStringList& value);
private:

    /** Common part of full event description*/
    static QString eventDescription(const QnAbstractBusinessActionPtr& action,
                                    const QnBusinessAggregationInfo& aggregationInfo = QnBusinessAggregationInfo());

    /** Details of event: aggregation info, date and time, other info */
    static QString eventDetails(const QnAbstractBusinessActionPtr& action,
                                const QnBusinessAggregationInfo& aggregationInfo = QnBusinessAggregationInfo(),
                                const QString& delimiter = QString());


    static QString resourceUrl(const QnBusinessEventParameters &params);
    static QString resourceName(const QnBusinessEventParameters &params);

    static QString conflictString(const QnBusinessEventParameters &params);

    static QString timestampString(const QnBusinessEventParameters &params, int aggregationCount);
};

#endif // __BUSINESS_STRINGS_HELPER_H__
