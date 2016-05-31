#pragma once

#include "actions/abstract_business_action.h"
#include "events/abstract_business_event.h"
#include "business_aggregation_info.h"
#include "business_fwd.h"

class QnBusinessStringsHelper: public QObject
{
    Q_OBJECT
public:
    static QString actionName(QnBusiness::ActionType value);

    /**
     * Name of the event in common, e.g. 'Motion on Camera'
     * Used primarily in lists where all event types are enumerated.
     */
    static QString eventName(QnBusiness::EventType value);

    /** Event <event> occurred on the <resource> */
    static QString eventAtResource(const QnBusinessEventParameters &params, Qn::ResourceInfoLevel detailLevel);
    /** Multiple <event> events occurred */
    static QString eventAtResources(const QnBusinessEventParameters &params);

    /**
     * @brief eventDescription      Form full event description, split to lines.
     * @param action                Action that describes the event.
     * @param aggregationInfo       Aggregation details if events were aggregated.
     * @param useIp                 Use resources addresses in the 'Source' field.
     * @param useHtml               Create html-formatted output.
     * @return                      Event description like this:
     *                                  Event: Motion on camera
     *                                  Source: Entrance Camera (192.168.0.5)
     *                                  Time: 5 times, first time at 15.00 on 19.06.2013
     *                                  ...
     */
    static QString eventDescription(const QnAbstractBusinessActionPtr& action,
                                    const QnBusinessAggregationInfo& aggregationInfo,
                                    Qn::ResourceInfoLevel detailLevel,
                                    bool useHtml);

    static QString eventDetailsWithTimestamp(const QnBusinessEventParameters &params, int aggregationCount, const QString& delimiter);
    static QString eventDetails(const QnBusinessEventParameters &params, const QString& delimiter);
    static QString renderEventDetailsText(QVariantHash contextMap);

    //TODO: #vasilenko isPublic field is not used, why?
    static QString urlForCamera(const QnUuid& id, qint64 timestampUsec, bool isPublic);

    static QString toggleStateToString(QnBusiness::EventState state);
    static QString eventTypeString(QnBusiness::EventType eventType,
                                   QnBusiness::EventState eventState,
                                   QnBusiness::ActionType actionType,
                                   const QnBusinessActionParameters &actionParams);
    static QString bruleDescriptionText(const QnBusinessEventRulePtr& bRule);
    static QnResourcePtr eventSource(const QnBusinessEventParameters &params);

    /** Details of event: aggregation info, date and time, other info */
    static QString aggregatedEventDetails(const QnAbstractBusinessActionPtr& action,
                                              const QnBusinessAggregationInfo& aggregationInfo,
                                              const QString& delimiter);
    static QString eventReason(const QnBusinessEventParameters& params);

    static QString eventTimestamp(const QnBusinessEventParameters &params, int aggregationCount);
    static QString eventTimestampShort(const QnBusinessEventParameters &params, int aggregationCount);

    static QString getResoureNameFromParams(const QnBusinessEventParameters& params,  Qn::ResourceInfoLevel detailLevel = Qn::RI_NameOnly);
};
