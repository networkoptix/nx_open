#ifndef __BUSINESS_RULE_PROCESSOR_H_
#define __BUSINESS_RULE_PROCESSOR_H_

#include <QtCore/QTimer>
#include <QtCore/QThread>
#include <QtCore/QMultiMap>

#include <core/resource/resource_fwd.h>

#include "business_message_bus.h"
#include "business_event_rule.h"

#include <business/business_aggregation_info.h>
#include <business/events/abstract_business_event.h>
#include <business/actions/abstract_business_action.h>
#include <business/actions/sendmail_business_action.h>
#include <business/actions/camera_output_business_action.h>

#include <nx_ec/ec_api.h>
#include <utils/common/request_param.h>

class QnProcessorAggregationInfo {
public:
    QnProcessorAggregationInfo():
        m_timestamp(0),
        m_initialized(false)
    {}

    /** Timestamp when the action should be executed. */
    qint64 estimatedEnd() {
        if (!m_initialized)
            return 0;
        qint64 period = m_rule->aggregationPeriod()*1000ll*1000ll;
        return m_timestamp + period;
    }

    void init(const QnAbstractBusinessEventPtr& event, const QnBusinessEventRulePtr& rule, qint64 timestamp) {
        m_event = event;
        m_rule = rule;
        m_timestamp = timestamp;
        m_initialized = true;
    }

    /** Restores the initial state. */
    void reset(qint64 timestamp){
        m_timestamp = timestamp;
        m_info.clear();
    }

    void append(const QnBusinessEventParameters& runtimeParams) {
        m_info.append(runtimeParams);
    }


    bool initialized() const {
        return m_initialized;
    }

    int totalCount() const {
        return m_info.totalCount();
    }

    QnAbstractBusinessEventPtr event() const {
        return m_event;
    }

    QnBusinessEventRulePtr rule() const {
        return m_rule;
    }

    const QnBusinessAggregationInfo& info() const {
        return m_info;
    }
private:
    QnAbstractBusinessEventPtr m_event;
    QnBusinessEventRulePtr m_rule;
    QnBusinessAggregationInfo m_info;

    /** Timestamp of the first event. */
    qint64 m_timestamp;

    /** Flag that event and rule are set. */
    bool m_initialized;
};

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

    bool broadcastBusinessAction(QnAbstractBusinessActionPtr action);
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
    static void fini();

protected slots:
    /*
    * Execute action physically. Return true if action success executed
    */
    virtual bool executeActionInternal(QnAbstractBusinessActionPtr action, QnResourcePtr res);
private slots:
    void at_broadcastBusinessActionFinished(int handle, ec2::ErrorCode errorCode);
    void at_sendEmailFinished(int handle, ec2::ErrorCode errorCode);
    void at_actionDelivered(QnAbstractBusinessActionPtr action);
    void at_actionDeliveryFailed(QnAbstractBusinessActionPtr  action);

    void at_businessRuleChanged(QnBusinessEventRulePtr bRule);
    void at_businessRuleDeleted(QnId id);
    void at_businessRuleReset(QnBusinessEventRuleList rules);

    void at_timer();


protected:
    virtual QImage getEventScreenshot(const QnBusinessEventParameters& params, QSize dstSize) const;
    
    bool containResource(QnResourceList resList, const QnId& resId) const;
    QnAbstractBusinessActionList matchActions(QnAbstractBusinessEventPtr bEvent);
    //QnBusinessMessageBus& getMessageBus() { return m_messageBus; }

    /*
    * Some actions can be executed on media server only. In this case, function returns media server there action must be executed
    */
    QnMediaServerResourcePtr getDestMServer(QnAbstractBusinessActionPtr action, QnResourcePtr res);

    void terminateRunningRule(QnBusinessEventRulePtr rule);

private:
    void at_businessRuleChanged_i(QnBusinessEventRulePtr bRule);

    bool sendMail(const QnSendMailBusinessActionPtr& action );

    QnAbstractBusinessActionPtr processToggleAction(QnAbstractBusinessEventPtr bEvent, QnBusinessEventRulePtr rule);
    QnAbstractBusinessActionPtr processInstantAction(QnAbstractBusinessEventPtr bEvent, QnBusinessEventRulePtr rule);
    bool checkRuleCondition(QnAbstractBusinessEventPtr bEvent, QnBusinessEventRulePtr rule) const;
    QString formatEmailList(const QStringList& value);

private:
    QList<QnBusinessEventRulePtr> m_rules;
    //QnBusinessMessageBus m_messageBus;
    static QnBusinessRuleProcessor* m_instance;

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

    QMap<QString, QnProcessorAggregationInfo> m_aggregateActions; // aggregation counter for instant actions
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
