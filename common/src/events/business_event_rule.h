#ifndef __ABSTRACT_BUSINESS_EVENT_RULE_H__
#define __ABSTRACT_BUSINESS_EVENT_RULE_H__

#include "abstract_business_event.h"
#include "abstract_business_action.h"
#include "business_logic_common.h"

#include "core/resource/resource.h"

/**
* This class define relation between business event and action
*/
class QnBusinessEventRule: public QnResource
{
    Q_OBJECT

public:
    QnBusinessEventRule();

    virtual QString getUniqueId() const override;

    // TODO: move to some factory
    QnAbstractBusinessActionPtr instantiateAction(QnAbstractBusinessEventPtr bEvent, ToggleState::Value tstate = ToggleState::NotDefined) const;


    BusinessEventType::Value eventType() const;
    void setEventType(const BusinessEventType::Value value);

    QnResourceList eventResources() const;
    void setEventResources(const QnResourceList &value);

    QnBusinessParams eventParams() const;
    void setEventParams(const QnBusinessParams& params);

    ToggleState::Value eventState() const;
    void setEventState(ToggleState::Value state);

    BusinessActionType::Value actionType() const;
    void setActionType(const BusinessActionType::Value value);

    QnResourceList actionResources() const;
    void setActionResources(const QnResourceList &value);

    QnBusinessParams actionParams() const;
    void setActionParams(const QnBusinessParams& params);

    /* action aggregation period in seconds */
    int aggregationPeriod() const;
    void setAggregationPeriod(int secs);

    QString schedule() const;
    void setSchedule(const QString value);

    QString comments() const;
    void setComments(const QString value);
private:
    //TODO: instant action + prolonged event: expose action when event starts or finishes
    //TODO: schedule
    BusinessEventType::Value m_eventType;
    QnResourceList m_eventResources;
    QnBusinessParams m_eventParams;
    ToggleState::Value m_eventState;

    BusinessActionType::Value m_actionType;
    QnResourceList m_actionResources;
    QnBusinessParams m_actionParams;

    int m_aggregationPeriod;

    QString m_schedule;
    QString m_comments;
};

typedef QnSharedResourcePointer<QnBusinessEventRule> QnBusinessEventRulePtr;
typedef QnSharedResourcePointerList<QnBusinessEventRule> QnBusinessEventRules;

Q_DECLARE_METATYPE(QnBusinessEventRulePtr)

#endif // __ABSTRACT_BUSINESS_EVENT_RULE_H__
