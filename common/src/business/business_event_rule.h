#ifndef __ABSTRACT_BUSINESS_EVENT_RULE_H__
#define __ABSTRACT_BUSINESS_EVENT_RULE_H__

#include <business/events/abstract_business_event.h>
#include <business/actions/abstract_business_action.h>
#include <business/business_logic_common.h>

#include <core/resource/resource_fwd.h>
#include "business_action_parameters.h"

/**
* This class define relation between business event and action
*/
class QnBusinessEventRule: public QObject
{
    Q_OBJECT

public:
    QnBusinessEventRule();
    ~QnBusinessEventRule();

    QString getUniqueId() const;

    int id() const;
    void setId(int value);

    BusinessEventType::Value eventType() const;
    void setEventType(const BusinessEventType::Value value);

    QnResourceList eventResources() const;
    void setEventResources(const QnResourceList &value);

    QnBusinessEventParameters eventParams() const;
    void setEventParams(const QnBusinessEventParameters& params);

    ToggleState::Value eventState() const;
    void setEventState(ToggleState::Value state);

    BusinessActionType::Value actionType() const;
    void setActionType(const BusinessActionType::Value value);

    QnResourceList actionResources() const;
    void setActionResources(const QnResourceList &value);

    QnBusinessActionParameters actionParams() const;
    void setActionParams(const QnBusinessActionParameters& params);

    /* action aggregation period in seconds */
    int aggregationPeriod() const;
    void setAggregationPeriod(int secs);

    bool disabled() const;
    void setDisabled(bool value);

    QString schedule() const;
    void setSchedule(const QString value);

    QString comments() const;
    void setComments(const QString value);

    /* Check if current time allowed in schedule */
    bool isScheduleMatchTime(const QDateTime& datetime) const;
private:
    //TODO: #GDM instant action + prolonged event: expose action when event starts or finishes
    //TODO: #GDM schedule
    int m_id;

    BusinessEventType::Value m_eventType;
    QnResourceList m_eventResources;
    QnBusinessEventParameters m_eventParams;
    ToggleState::Value m_eventState;

    BusinessActionType::Value m_actionType;
    QnResourceList m_actionResources;
    QnBusinessActionParameters m_actionParams;

    int m_aggregationPeriod;
    bool m_disabled;

    QString m_schedule;
    QByteArray m_binSchedule;
    QString m_comments;
};

typedef QSharedPointer<QnBusinessEventRule> QnBusinessEventRulePtr;
typedef QList<QnBusinessEventRulePtr> QnBusinessEventRules; // TODO: #Elric QnBusinessEventRuleList

Q_DECLARE_METATYPE(QnBusinessEventRulePtr)

#endif // __ABSTRACT_BUSINESS_EVENT_RULE_H__
