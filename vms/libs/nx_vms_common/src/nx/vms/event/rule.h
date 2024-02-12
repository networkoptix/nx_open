// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource/resource_fwd.h>
#include <nx/vms/event/action_parameters.h>
#include <nx/vms/event/actions/abstract_action.h>
#include <nx/vms/event/event_fwd.h>
#include <nx/vms/event/events/abstract_event.h>

namespace nx {
namespace vms {
namespace event {

/**
* This class define relation between event and action
*/
class NX_VMS_COMMON_API Rule: public QObject
{
public:
    Rule();
    virtual ~Rule() override;

    static const QByteArray kGuidPostfix;

    QString getUniqueId() const;

    nx::Uuid id() const;
    void setId(const nx::Uuid& value);

    EventType eventType() const;
    void setEventType(EventType eventType);

    QVector<nx::Uuid> eventResources() const;
    void setEventResources(const QVector<nx::Uuid> &value);

    EventParameters eventParams() const;
    void setEventParams(const EventParameters& params);

    EventState eventState() const;
    void setEventState(EventState state);

    ActionType actionType() const;
    void setActionType(ActionType actionType);

    QVector<nx::Uuid> actionResources() const;
    void setActionResources(const QVector<nx::Uuid> &value);

    ActionParameters actionParams() const;
    void setActionParams(const ActionParameters& params);

    bool isActionProlonged() const;

    /* action aggregation period in seconds */
    int aggregationPeriod() const;
    void setAggregationPeriod(int seconds);

    bool isDisabled() const;
    void setDisabled(bool disabled);

    QString schedule() const;
    void setSchedule(const QString &schedule);

    QString comment() const;
    void setComment(const QString &comment);

    bool isSystem() const;
    void setSystem(bool system);

    /* Check if current time allowed in schedule */
    bool isScheduleMatchTime(const QDateTime& datetime) const;

    static RuleList getDefaultRules();

    static RuleList getNotificationRules();
    static RuleList getSystemRules();
    static RuleList getRulesUpd43();
    static RuleList getPluginDiagnosticEventUpdateRules();
    static RuleList getServerCertificateErrorRules();
    static RuleList getLdapSyncIssueRules();

    /**
     * Some guids in getSystemRules() didn't match to the original SQL script. It is fixed already,
     * but an old database could has these invalid guids.
     * @returns Map of valid guids by old (invalid) guids.
     */
    static QMap<nx::Uuid, nx::Uuid> remappedGuidsToFix();
    static RuleList getDisabledRulesUpd43();

    Rule* clone();
    void removeResource(const nx::Uuid& resId);

private:
    Rule(int internalId, int aggregationPeriod,  bool isSystem, ActionType bActionType,
        EventType bEventType, const QList<nx::Uuid>& subjectIds = {}, bool allUsers = false);

    nx::Uuid m_id;

    EventType m_eventType;
    QVector<nx::Uuid> m_eventResources;
    EventParameters m_eventParams;
    EventState m_eventState;

    ActionType m_actionType;
    QVector<nx::Uuid> m_actionResources;
    ActionParameters m_actionParams;

    int m_aggregationPeriod;
    bool m_disabled;

    QString m_schedule;
    QByteArray m_binSchedule;
    QString m_comment;

    bool m_system;
};

} // namespace event
} // namespace vms
} // namespace nx
