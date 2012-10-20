#ifndef __ABSTRACT_BUSINESS_EVENT_RULE_H__
#define __ABSTRACT_BUSINESS_EVENT_RULE_H__

#include "abstract_business_event.h"
#include "abstract_business_action.h"

/*
* This class define relation between business event and action
*/

class QnAbstractBusinessEventRule
{
public:
protected:
    /*
    * Check addition condition between event and action
    */
    virtual bool checkCondtition() = 0;
private:
    BusinessEventType m_eventType;
    QnResourcePtr m_source;

    BusinessActionType m_actionType;
    QnResourcePtr m_destination;
};

typedef QSharedPointer<QnAbstractBusinessEventRule> QnAbstractBusinessEventRulePtr;

#endif // __ABSTRACT_BUSINESS_EVENT_RULE_H__
