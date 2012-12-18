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
    void setEventType(BusinessEventType::Value value);

    QnResourcePtr eventResource() const;
    void setEventResource(QnResourcePtr value);

    QnBusinessParams eventParams() const;
    void setEventParams(const QnBusinessParams& params);

    BusinessActionType::Value actionType() const;
    void setActionType(BusinessActionType::Value value);

    QnResourcePtr actionResource() const;
    void setActionResource(QnResourcePtr value);

    QnBusinessParams actionParams() const;
    void setActionParams(const QnBusinessParams& params);
private:
    //TODO: instant action + prolonged event: expose action when event starts or finishes
    //TODO: schedule
    BusinessEventType::Value m_eventType;
    QnResourcePtr m_eventResource;
    QnBusinessParams m_eventParams;

    BusinessActionType::Value m_actionType;
    QnResourcePtr m_actionResource;
    QnBusinessParams m_actionParams;
};

typedef QnSharedResourcePointer<QnBusinessEventRule> QnBusinessEventRulePtr;
typedef QnSharedResourcePointerList<QnBusinessEventRule> QnBusinessEventRules;

#endif // __ABSTRACT_BUSINESS_EVENT_RULE_H__
