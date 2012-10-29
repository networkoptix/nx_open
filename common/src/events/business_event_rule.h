#ifndef __ABSTRACT_BUSINESS_EVENT_RULE_H__
#define __ABSTRACT_BUSINESS_EVENT_RULE_H__

#include "abstract_business_event.h"
#include "abstract_business_action.h"
#include "business_logic_common.h"
#include "core/resource/resource.h"

/*
* This class define relation between business event and action
*/

class QnBusinessEventRule: public QnResource
{
public:

    QnBusinessEventRule();

    virtual QString getUniqueId() const override;

    QnAbstractBusinessActionPtr getAction(QnAbstractBusinessEventPtr bEvent, ToggleState tstate = ToggleState_NotDefined);
    QnBusinessParams getEventCondition() const { return m_eventCondition; }

    QnResourcePtr getSrcResource() const { return m_source; }
    void setSrcResource(QnResourcePtr value) { m_source = value; }

    BusinessEventType getEventType() const { return m_eventType; }
    void setEventType(BusinessEventType value) { m_eventType = value; }

    BusinessActionType getActionType() const { return m_actionType; }
    void setActionType(BusinessActionType value) { m_actionType = value; }

    QnResourcePtr getDstResource() const { return m_destination; }
    void setDstResource(QnResourcePtr value) { m_destination = value; }

    void getBusinessParams() const;
    void setBusinessParams(const QnBusinessParams& params);

    /*
    * Return true if last returned action is toggledAction and action has ToggleState_On
    */
    bool isActionInProgress() const;
private:
    BusinessEventType m_eventType;
    QnResourcePtr m_source;
    QnBusinessParams m_eventCondition;

    BusinessActionType m_actionType;
    QnResourcePtr m_destination;
    QnBusinessParams m_actionParams;

    bool m_actionInProgress;
};

typedef QSharedPointer<QnBusinessEventRule> QnBusinessEventRulePtr;
typedef QList<QnBusinessEventRulePtr> QnBusinessEventRules;

#endif // __ABSTRACT_BUSINESS_EVENT_RULE_H__
