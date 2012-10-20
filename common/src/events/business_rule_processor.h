#ifndef __BUSINESS_RULE_PROCESSOR_H_
#define __BUSINESS_RULE_PROCESSOR_H_

#include <QMultiMap>
#include "abstract_business_event.h"
#include "abstract_business_action.h"
#include "business_message_bus.h"
#include "abstract_business_event_rule.h"

/*
* This class route business event and generate business action
*/

class QnBusinessRuleProcessor: public QThread
{
public:
public slots:
    /*
    * This function matches all business actions for specified business event and execute it
    */
    void processBusinessEvent(QnAbstractBusinessEventPtr bEvent);
protected:
    QList <QnAbstractBusinessActionPtr> matchActions();
    void executeAction(QnAbstractBusinessActionPtr action);
private:
    QMultiMap<QByteArray, QnAbstractBusinessEventRulePtr> m_rules;
    QnBusinessMessageBus m_messageBus;
};

#endif // __BUSINESS_RULE_PROCESSOR_H_
