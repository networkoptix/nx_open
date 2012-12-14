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
    Q_OBJECT

public:
    QnBusinessEventRule();

    virtual QString getUniqueId() const override;

    QnAbstractBusinessActionPtr instantiateAction(QnAbstractBusinessEventPtr bEvent, ToggleState::Value tstate = ToggleState::NotDefined) const;

    /*!
        \param name This should be a constant from \a BusinessEventParameters namespace
    */
    void addEventCondition( const QString& name, const QVariant& value ) { m_eventCondition[name] = value; }
    void addEventConditions(QnBusinessParams conditions) {
        //TODO: implement me
    }

    void clearEventConditions() { m_eventCondition.clear();  }
    QnBusinessParams eventConditions() const { return m_eventCondition; }

    QnResourcePtr getSrcResource() const;
    void setSrcResource(QnResourcePtr value);

    BusinessEventType::Value getEventType() const;
    void setEventType(BusinessEventType::Value value);

    BusinessActionType::Value getActionType() const;
    void setActionType(BusinessActionType::Value value);

    QnResourcePtr getDstResource() const;
    void setDstResource(QnResourcePtr value);

    QnBusinessParams getBusinessParams() const;
    void setBusinessParams(const QnBusinessParams& params);

    ToggleState::Value getEventToggleState() const;
    void setEventToggleState(ToggleState::Value value);
private:
    //TODO: instant action + prolonged event: expose action when event starts or finishes
    //TODO: schedule
    BusinessEventType::Value m_eventType;
    QnResourcePtr m_source;
    QnBusinessParams m_eventCondition;

    BusinessActionType::Value m_actionType;
    QnResourcePtr m_destination;
    QnBusinessParams m_actionParams;
};

typedef QnSharedResourcePointer<QnBusinessEventRule> QnBusinessEventRulePtr;
typedef QnSharedResourcePointerList<QnBusinessEventRule> QnBusinessEventRules;

#endif // __ABSTRACT_BUSINESS_EVENT_RULE_H__
