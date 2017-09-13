#pragma once

#include <common/common_module_aware.h>

#include <nx/vms/event/actions/abstract_action.h>
#include <nx/vms/event/events/abstract_event.h>
#include <nx/vms/event/aggregation_info.h>
#include <nx/vms/event/event_fwd.h>

namespace nx {
namespace vms {
namespace event {

class StringsHelper: public QObject, public QnCommonModuleAware
{
    Q_OBJECT
    using base_type = QObject;

public:
    StringsHelper(QnCommonModule* commonModule);

    QString actionName(ActionType value) const;

    /**
     * Name of the event in common, e.g. 'Motion on Camera'
     * Used primarily in lists where all event types are enumerated.
     */
    QString eventName(EventType value, int count = 1) const;

    /** Event <event> occurred on the <resource> */
    QString eventAtResource(const EventParameters &params,
        Qn::ResourceInfoLevel detailLevel) const;

    /** Multiple <event> events occurred */
    QString eventAtResources(const EventParameters &params) const;

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
    QStringList eventDescription(const AbstractActionPtr& action,
                                    const AggregationInfo& aggregationInfo,
                                    Qn::ResourceInfoLevel detailLevel) const;

    QStringList eventDetailsWithTimestamp(const EventParameters &params,
        int aggregationCount) const;

    QStringList eventDetails(const EventParameters &params) const;

    // TODO: #vasilenko isPublic field is not used, why?
    QString urlForCamera(const QnUuid& id, qint64 timestampUsec, bool isPublic) const;

    QString toggleStateToString(EventState state) const;
    QString eventTypeString(EventType eventType,
                                   EventState eventState,
                                   ActionType actionType,
                                   const ActionParameters &actionParams) const;
    QString ruleDescriptionText(const RulePtr& rule) const;
    QnResourcePtr eventSource(const EventParameters &params) const;

    /** Details of event: aggregation info, date and time, other info, split by lines. */
    QStringList aggregatedEventDetails(
        const AbstractActionPtr& action,
        const AggregationInfo& aggregationInfo) const;

    QString eventReason(const EventParameters& params) const;

    QString eventTimestamp(const EventParameters &params, int aggregationCount) const;
	QString eventTimestampTime(const EventParameters &params) const;
	QString eventTimestampDate(const EventParameters &params) const;

    QString eventTimestampShort(const EventParameters &params, int aggregationCount) const;


	QString getResoureNameFromParams(const EventParameters& params,
        Qn::ResourceInfoLevel detailLevel) const;

	QString getResoureIPFromParams(const EventParameters& params) const;

    // Argument showName controls showing specific subject name in case of just one subject.
    QString actionSubjects(const RulePtr& rule, bool showName = true) const;
    QString actionSubjects(const QnUserResourceList& users, const QList<QnUuid>& roles,
        bool showName = true) const;

    static QString allUsersText();
    static QString needToSelectUserText();

    static QString defaultSoftwareTriggerName();
    static QString getSoftwareTriggerName(const QString& id);
    static QString getSoftwareTriggerName(const EventParameters& params);

    QString getAnalyticsSdkEventName(const EventParameters& params,
        const QString& locale = QString()) const;
};

} // namespace event
} // namespace vms
} // namespace nx
