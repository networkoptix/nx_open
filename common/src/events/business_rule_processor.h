#ifndef __BUSINESS_RULE_PROCESSOR_H_
#define __BUSINESS_RULE_PROCESSOR_H_

#include <QTimer>
#include <QThread>
#include <QMultiMap>
#include "core/resource/resource_fwd.h"
#include "abstract_business_event.h"
#include "abstract_business_action.h"
#include "business_message_bus.h"
#include "business_event_rule.h"
#include "sendmail_business_action.h"
#include "camera_output_business_action.h"
#include "popup_business_action.h"

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
    void at_businessRuleDeleted(QnId id);

private slots:
    void at_actionDelivered(QnAbstractBusinessActionPtr action);
    void at_actionDeliveryFailed(QnAbstractBusinessActionPtr  action);

    void at_timer();
protected:
    bool containResource(QnResourceList resList, const QnId& resId) const;
    QList <QnAbstractBusinessActionPtr> matchActions(QnAbstractBusinessEventPtr bEvent);
    //QnBusinessMessageBus& getMessageBus() { return m_messageBus; }

    /*
    * Execute action physically. Return true if action success executed
    */
    virtual bool executeActionInternal(QnAbstractBusinessActionPtr action) = 0;

    /*
    * Some actions can be executed on media server only. In this case, function returns media server there action must be executed
    */
    QnMediaServerResourcePtr getDestMServer(QnAbstractBusinessActionPtr action, QnResourcePtr res);

private:
    QList<QnBusinessEventRulePtr> m_rules;
    //QnBusinessMessageBus m_messageBus;
    static QnBusinessRuleProcessor* m_instance;

    //TODO: move to mserver_business_rule_processor
    bool triggerCameraOutput( const QnCameraOutputBusinessActionPtr& action );
    bool triggerCameraOutput( const QnCameraOutputBusinessActionPtr& action, QnResourcePtr resource );

    bool sendMail( const QnSendMailBusinessActionPtr& action );

    bool showPopup(QnPopupBusinessActionPtr action);


    QnAbstractBusinessActionPtr processToggleAction(QnAbstractBusinessEventPtr bEvent, QnBusinessEventRulePtr rule);
    QnAbstractBusinessActionPtr processInstantAction(QnAbstractBusinessEventPtr bEvent, QnBusinessEventRulePtr rule);

    /**
     * @brief m_rulesInProgress         Stores actions that are toggled and state is On
     */
    QSet<QString> m_rulesInProgress;

    struct QAggregationInfo 
    {
        QAggregationInfo(): timeStamp(0), count(0) {}
        QAggregationInfo(qint64 _timeStamp, qint64 _count): timeStamp(_timeStamp), count(_count) {}
        qint64 timeStamp;
        int count;
        QnAbstractBusinessEventPtr bEvent;
        QnBusinessEventRulePtr bRule;
    };
    QMap<QString, QAggregationInfo> m_aggregateActions;
    mutable QMutex m_mutex;
    QTimer m_timer;

    /*!
        \param isRuleAdded \a true - rule added, \a false - removed
    */
    void notifyResourcesAboutEventIfNeccessary( QnBusinessEventRulePtr businessRule, bool isRuleAdded );
};

#define qnBusinessRuleProcessor QnBusinessRuleProcessor::instance()

#endif // __BUSINESS_RULE_PROCESSOR_H_
