#ifndef __BUSINESS_RULE_PROCESSOR_H_
#define __BUSINESS_RULE_PROCESSOR_H_

#include <QTimer>
#include <QThread>
#include <QMultiMap>

#include <core/resource/resource_fwd.h>

#include "business_message_bus.h"
#include "business_event_rule.h"

#include <business/events/abstract_business_event.h>
#include <business/actions/abstract_business_action.h>
#include <business/actions/sendmail_business_action.h>
#include <business/actions/camera_output_business_action.h>
#include <business/actions/popup_business_action.h>

/*
* This class route business event and generate business action
*/

class QnBusinessRuleProcessor: public QThread
{
    Q_OBJECT
public:
    QnBusinessRuleProcessor();
    virtual ~QnBusinessRuleProcessor();

    void addBusinessRule(QnBusinessEventRulePtr value);
    
    
    /*
    * Return module GUID. if destination action intended for current module, no route through message bus is required
    */

    virtual QString getGuid() const { return QString(); }
public slots:
    /*
    * This function matches all business actions for specified business event and execute it
    * So, call this function if business event occured
    */
    void processBusinessEvent(QnAbstractBusinessEventPtr bEvent);

    /*
    * Execute business action.
    * This function is called if business event already matched to action(s).
    */
    void executeAction(QnAbstractBusinessActionPtr action);

    static QnBusinessRuleProcessor* instance();
    static void init(QnBusinessRuleProcessor* instance);

    void at_businessRuleChanged(QnBusinessEventRulePtr bRule);
    void at_businessRuleDeleted(int id);
protected slots:
    /*
    * Execute action physically. Return true if action success executed
    */
    virtual bool executeActionInternal(QnAbstractBusinessActionPtr action, QnResourcePtr res);
private slots:
    void at_sendEmailFinished(int status, const QByteArray& errorString, bool result, int handle);
    void at_actionDelivered(QnAbstractBusinessActionPtr action);
    void at_actionDeliveryFailed(QnAbstractBusinessActionPtr  action);

    void at_timer();


protected:
    bool containResource(QnResourceList resList, const QnId& resId) const;
    QList <QnAbstractBusinessActionPtr> matchActions(QnAbstractBusinessEventPtr bEvent);
    //QnBusinessMessageBus& getMessageBus() { return m_messageBus; }

    /*
    * Some actions can be executed on media server only. In this case, function returns media server there action must be executed
    */
    QnMediaServerResourcePtr getDestMServer(QnAbstractBusinessActionPtr action, QnResourcePtr res);

    void terminateRunningRule(QnBusinessEventRulePtr rule);
private:
    QList<QnBusinessEventRulePtr> m_rules;
    //QnBusinessMessageBus m_messageBus;
    static QnBusinessRuleProcessor* m_instance;

    bool sendMail( const QnSendMailBusinessActionPtr& action );

    bool showPopup(QnPopupBusinessActionPtr action);


    QnAbstractBusinessActionPtr processToggleAction(QnAbstractBusinessEventPtr bEvent, QnBusinessEventRulePtr rule);
    QnAbstractBusinessActionPtr processInstantAction(QnAbstractBusinessEventPtr bEvent, QnBusinessEventRulePtr rule);
    bool checkRuleCondition(QnAbstractBusinessEventPtr bEvent, QnBusinessEventRulePtr rule) const;

    struct RunningRuleInfo
    {
        RunningRuleInfo() {}
        QMap<QnId, QnAbstractBusinessEventPtr> resources; 
        QSet<QnId> isActionRunning; // actions that has been started by resource. Continues action starts only onces for all event resources.
    };
    typedef QMap<QString, RunningRuleInfo> RunningRuleMap;

    /**
     * @brief m_eventsInProgress         Stores events that are toggled and state is On
     */
    RunningRuleMap m_rulesInProgress;


    /**
     * @brief match resources between event and rule
     */
    bool checkEventCondition(QnAbstractBusinessEventPtr bEvent, QnBusinessEventRulePtr rule);

    struct QAggregationInfo 
    {
        QAggregationInfo(): timeStamp(0), count(0) {}
        QAggregationInfo(qint64 _timeStamp, qint64 _count): timeStamp(_timeStamp), count(_count) {}
        qint64 timeStamp;
        int count;
        QnAbstractBusinessEventPtr bEvent;
        QnBusinessEventRulePtr bRule;
    };
    QMap<QString, QAggregationInfo> m_aggregateActions; // aggregation counter for instant actions
    QMap<QString, int> m_actionInProgress;              // remove duplicates for long actions
    mutable QMutex m_mutex;
    QTimer m_timer;

    /*!
        \param isRuleAdded \a true - rule added, \a false - removed
    */
    void notifyResourcesAboutEventIfNeccessary( QnBusinessEventRulePtr businessRule, bool isRuleAdded );
};

#define qnBusinessRuleProcessor QnBusinessRuleProcessor::instance()

#endif // __BUSINESS_RULE_PROCESSOR_H_
