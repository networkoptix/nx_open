#pragma once

#include <nx/vms/event/action_parameters.h>
#include <nx/vms/event/actions/abstract_action.h>
#include <nx/vms/event/events/abstract_event.h>
#include <nx/vms/event/event_fwd.h>

#include <core/resource/resource_fwd.h>

namespace nx {
namespace vms {
namespace event {

/**
* This class define relation between event and action
*/
class Rule: public QObject
{
    Q_OBJECT

public:
    Rule();
    virtual ~Rule() override;

    static const QByteArray kGuidPostfix;

    QString getUniqueId() const;

    QnUuid id() const;
    void setId(const QnUuid& value);

    EventType eventType() const;
    void setEventType(EventType eventType);

    QVector<QnUuid> eventResources() const;
    void setEventResources(const QVector<QnUuid> &value);

    EventParameters eventParams() const;
    void setEventParams(const EventParameters& params);

    EventState eventState() const;
    void setEventState(EventState state);

    ActionType actionType() const;
    void setActionType(ActionType actionType);

    QVector<QnUuid> actionResources() const;
    void setActionResources(const QVector<QnUuid> &value);

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

    /**
     * Some guids in getSystemRules() didn't match to original SQL script.
     * It is fixed now, but old database could has these invalid guids.
     * return map of invalid and valid guids. map key contains old guid.
     */
    static QMap<QnUuid, QnUuid> remappedGuidsToFix();
    static RuleList getSystemRules();
    static RuleList getRulesUpd43();
    static RuleList getRulesUpd48();

    Rule* clone();
    void removeResource(const QnUuid& resId);

private:
    Rule(int internalId, int aggregationPeriod,  bool isSystem, ActionType bActionType,
        EventType bEventType, const QList<QnUuid>& subjectIds = {}, bool allUsers = false);

    QnUuid m_id;

    EventType m_eventType;
    QVector<QnUuid> m_eventResources;
    EventParameters m_eventParams;
    EventState m_eventState;

    ActionType m_actionType;
    QVector<QnUuid> m_actionResources;
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

Q_DECLARE_METATYPE(nx::vms::event::RulePtr)
Q_DECLARE_METATYPE(nx::vms::event::RuleList)
