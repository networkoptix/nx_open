#pragma once

#include <common/common_module_aware.h>

#include "actions/abstract_business_action.h"
#include "events/abstract_business_event.h"
#include "business_aggregation_info.h"
#include "business_fwd.h"

class QnBusinessStringsHelper: public QObject, public QnCommonModuleAware
{
    Q_OBJECT
    using base_type = QObject;
public:
    QnBusinessStringsHelper(QnCommonModule* commonModule);

    QString actionName(QnBusiness::ActionType value) const;

    /**
     * Name of the event in common, e.g. 'Motion on Camera'
     * Used primarily in lists where all event types are enumerated.
     */
    QString eventName(QnBusiness::EventType value, int count = 1) const;

    /** Event <event> occurred on the <resource> */
    QString eventAtResource(const QnBusinessEventParameters &params,
        Qn::ResourceInfoLevel detailLevel) const;

    /** Multiple <event> events occurred */
    QString eventAtResources(const QnBusinessEventParameters &params) const;

    /**
     * @brief eventDescription      Form full event description, split to lines.
     * @param action                Action that describes the event.
     * @param aggregationInfo       Aggregation details if events were aggregated.
     * @param detailLevel           Format of resource names.
     * @return                      Event description like this:
     *                                  Event: Motion on camera
     *                                  Source: Entrance Camera (192.168.0.5)
     *                                  Time: 5 times, first time at 15.00 on 19.06.2013
     *                                  ...
     */
    QStringList eventDescription(const QnAbstractBusinessActionPtr& action,
                                    const QnBusinessAggregationInfo& aggregationInfo,
                                    Qn::ResourceInfoLevel detailLevel) const;

    QStringList eventDetailsWithTimestamp(const QnBusinessEventParameters &params,
        int aggregationCount) const;

    QStringList eventDetails(const QnBusinessEventParameters &params) const;

    //TODO: #vasilenko isPublic field is not used, why?
    QString urlForCamera(const QnUuid& id, qint64 timestampUsec, bool isPublic) const;

    QString toggleStateToString(QnBusiness::EventState state) const;
    QString eventTypeString(QnBusiness::EventType eventType,
                                   QnBusiness::EventState eventState,
                                   QnBusiness::ActionType actionType,
                                   const QnBusinessActionParameters &actionParams) const;
    QString bruleDescriptionText(const QnBusinessEventRulePtr& bRule) const;
    QnResourcePtr eventSource(const QnBusinessEventParameters &params) const;

    /** Details of event: aggregation info, date and time, other info, split by lines. */
    QStringList aggregatedEventDetails(
        const QnAbstractBusinessActionPtr& action,
        const QnBusinessAggregationInfo& aggregationInfo) const;

    QString eventReason(const QnBusinessEventParameters& params) const;

    QString eventTimestamp(const QnBusinessEventParameters &params, int aggregationCount) const;
	QString eventTimestampTime(const QnBusinessEventParameters &params) const;
	QString eventTimestampDate(const QnBusinessEventParameters &params) const;

    QString eventTimestampShort(const QnBusinessEventParameters &params, int aggregationCount) const;


	QString getResoureNameFromParams(const QnBusinessEventParameters& params,
        Qn::ResourceInfoLevel detailLevel) const;

	QString getResoureIPFromParams(const QnBusinessEventParameters& params) const;

    // Argument showName controls showing specific subject name in case of just one subject.
    QString actionSubjects(const QnBusinessEventRulePtr& rule, bool showName = true) const;
    QString actionSubjects(const QnUserResourceList& users, const QList<QnUuid>& roles,
        bool showName = true) const;

    static QString defaultSoftwareTriggerName();
    static QString getSoftwareTriggerName(const QString& id);
    static QString getSoftwareTriggerName(const QnBusinessEventParameters& params);
};
