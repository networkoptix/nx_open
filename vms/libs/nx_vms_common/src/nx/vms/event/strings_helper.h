// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>

#include <nx/vms/api/data/user_group_data.h>
#include <nx/vms/common/system_context_aware.h>
#include <nx/vms/event/actions/abstract_action.h>
#include <nx/vms/event/events/abstract_event.h>

#include "event_fwd.h"

namespace nx {
namespace vms {
namespace event {

class AggregationInfo;

enum class AttrSerializePolicy
{
    none,
    singleLine,
    multiLine
};

class NX_VMS_COMMON_API StringsHelper:
    public QObject,
    public /*mixin*/ common::SystemContextAware
{
    Q_OBJECT
    using base_type = QObject;

public:
    StringsHelper(common::SystemContext* context);

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
     * Used as notification tooltip only.
     * @param action                Action that describes the event.
     * @param aggregationInfo       Aggregation details if events were aggregated.
     * @param detailLevel           Format of resource names.
     * @param policy                Analytics attributes formatting policy.
     * @return                      Event description like this:
     *                                  Event: Motion on camera
     *                                  Source: Entrance Camera (192.168.0.5)
     *                                  Time: 5 times, first time at 15.00 on 19.06.2013
     *                                  ...
     */
    QStringList eventDescription(const AbstractActionPtr& action,
        const AggregationInfo& aggregationInfo,
        Qn::ResourceInfoLevel detailLevel,
        AttrSerializePolicy policy) const;
    QStringList eventDetails(
        const EventParameters& params, AttrSerializePolicy policy) const;

    /**
     * Construct url, opening webadmin view for the given camera at the given time.
     * @param id Camera id.
     * @param timestamp Timestamp.
     * @param usePublicIp If the url is formed on the server side, we can force public server ip to
     *     be used instead of the local one. Actual for server-side only.
     * @param proxyAddress Force proxy address to be used. Actual for the client side only.
     */
    QString urlForCamera(
        const QnUuid& id,
        std::chrono::milliseconds timestamp,
        bool usePublicIp,
        const std::optional<nx::network::SocketAddress>& proxyAddress = std::nullopt) const;

    QString toggleStateToString(EventState state) const;
    QString ruleDescriptionText(const RulePtr& rule) const;
    QString sourceCameraCheckboxText(ActionType actionType) const;

    QString eventTypeString(
        EventType eventType,
        EventState eventState,
        ActionType actionType,
        const ActionParameters& actionParams) const;

    QnResourcePtr eventSource(const EventParameters &params) const;
    QString eventReason(const EventParameters& params) const;

    QString eventTimestampInHtml(const EventParameters &params, int aggregationCount) const;
    QString eventTimestampTime(const EventParameters &params) const;
    QString eventTimestampDate(const EventParameters &params) const;

    QString getResoureNameFromParams(const EventParameters& params,
        Qn::ResourceInfoLevel detailLevel) const;

    QString getResoureIPFromParams(const EventParameters& params) const;

    // Argument showName controls showing specific subject name in case of just one subject.
    QString actionSubjects(const RulePtr& rule, bool showName = true) const;
    QString actionSubjects(const QnUserResourceList& users,
        const nx::vms::api::UserGroupDataList& groups,
        bool showName = true) const;

    static QString allUsersText();
    static QString needToSelectUserText();

    static QString defaultSoftwareTriggerName();
    static QString getSoftwareTriggerName(const QString& name);
    static QString getSoftwareTriggerName(const EventParameters& params);

    static QString poeConsumption();
    static QString poeConsumptionString(double current, double limit);
    static QString poeOverallConsumptionString(double current, double limit);
    static QString poeConsumptionStringFromParams(const EventParameters& params);

    static QString backupResultText(const EventParameters& params);
    static QString backupTimeText(const QDateTime& time);

    static QString ldapSyncIssueReason(
        EventReason reasonCode,
        std::optional<std::chrono::seconds> syncInterval);
    static QString ldapSyncIssueText(const EventParameters& params);

    QString getAnalyticsSdkEventName(const EventParameters& params,
        const QString& locale = QString()) const;
    QString getAnalyticsSdkObjectName(const EventParameters& params,
        const QString& locale = QString()) const;

    QString notificationCaption(
        const EventParameters& parameters,
        const QnVirtualCameraResourcePtr& camera,
        bool useHtml = true) const;

    QString notificationDescription(const EventParameters& parameters) const;

private:
    QString eventTimestamp(const EventParameters &params, int aggregationCount) const;

    QStringList eventDetailsWithTimestamp(const EventParameters &params,
        int aggregationCount, AttrSerializePolicy policy) const;

    /** Details of event: aggregation info, date and time, other info, split by lines. */
    QStringList aggregatedEventDetails(
        const AbstractActionPtr& action,
        const AggregationInfo& aggregationInfo,
        AttrSerializePolicy policy) const;
};

} // namespace event
} // namespace vms
} // namespace nx
