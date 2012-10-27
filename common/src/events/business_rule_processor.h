#ifndef __BUSINESS_RULE_PROCESSOR_H_
#define __BUSINESS_RULE_PROCESSOR_H_

#include <QThread>
#include <QMultiMap>
#include "abstract_business_event.h"
#include "abstract_business_action.h"
#include "business_message_bus.h"
#include "business_event_rule.h"

/*
* This class route business event and generate business action
*/

class QnBusinessRuleProcessor: public QThread
{
public:
    void addBusinessRule(QnAbstractBusinessEventRulePtr value);
public slots:
    /*
    * This function matches all business actions for specified business event and execute it
    */
    void processBusinessEvent(QnAbstractBusinessEventPtr bEvent);

    static QnBusinessRuleProcessor* instance();
    static void init(QnBusinessRuleProcessor* instance);
protected:
    QList <QnAbstractBusinessActionPtr> matchActions(QnAbstractBusinessEventPtr bEvent);
    QnBusinessMessageBus& getMessageBus() { return m_messageBus; }
    void executeAction(QnAbstractBusinessActionPtr action);

    /*
    * Execute action physically. Return true if action success executed
    */
    virtual bool executeActionInternal(QnAbstractBusinessActionPtr action) = 0;

    /*
    * Some actions can be executed on media server only. In this case, function returns media server there action must be executed
    */
    QnMediaServerResourcePtr getDestMServer(QnAbstractBusinessActionPtr action);
private:
    QList<QnAbstractBusinessEventRulePtr> m_rules;
    QnBusinessMessageBus m_messageBus;
    static QnBusinessRuleProcessor* m_instance;
};

#define bRuleProcessor QnBusinessRuleProcessor::instance()

#endif // __BUSINESS_RULE_PROCESSOR_H_
