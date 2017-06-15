#ifndef __BUSINESS_RULE_PROCESSOR_H_
#define __BUSINESS_RULE_PROCESSOR_H_

#include <nx/utils/thread/mutex.h>
#include <QtCore/QTimer>
#include <QtCore/QThread>
#include <QtCore/QMultiMap>
#include <QElapsedTimer>

#include <core/resource/resource_fwd.h>

#include "business_message_bus.h"
#include "business/business_event_rule.h"

#include <business/business_aggregation_info.h>
#include <business/events/abstract_business_event.h>
#include <business/actions/abstract_business_action.h>
#include <business/actions/sendmail_business_action.h>
#include <business/actions/camera_output_business_action.h>

#include <nx_ec/ec_api.h>
#include <utils/common/request_param.h>
#include <common/common_module_aware.h>
#include <nx/utils/singleton.h>

class EmailManagerImpl;

class QnProcessorAggregationInfo {
public:
    QnProcessorAggregationInfo():
        m_initialized(false)
    {
        m_timer.invalidate();
    }

    /** Timestamp when the action should be executed. */
    bool isExpired() {
        if (m_timer.isValid())
            return m_timer.hasExpired(m_rule->aggregationPeriod()*1000ll);
        else
            return true;
    }

    void init(const QnAbstractBusinessEventPtr& event, const QnBusinessEventRulePtr& rule) {
        m_event = event;
        m_rule = rule;
        m_initialized = true;
    }

    /** Restores the initial state. */
    void reset(){
        m_timer.restart();
        m_info.clear();
        m_initialized = false;
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

    /** Agregation timer */
    QElapsedTimer m_timer;

    /** Flag that event and rule are set. */
    bool m_initialized;
};

/*
* This class route business event and generate business action
*/
class QnBusinessRuleProcessor:
    public QThread,
    public Singleton<QnBusinessRuleProcessor>,
    public QnCommonModuleAware
{
    Q_OBJECT
public:
    QnBusinessRuleProcessor(QnCommonModule* commonModule);
    virtual ~QnBusinessRuleProcessor();

    void addBusinessRule(const QnBusinessEventRulePtr& value);


    /*
    * Return module GUID. if destination action intended for current module, no route through message bus is required
    */

    virtual QnUuid getGuid() const { return QnUuid(); }

    bool broadcastBusinessAction(const QnAbstractBusinessActionPtr& action);
public slots:
    /*
    * This function matches all business actions for specified business event and execute it
    * So, call this function if business event occurred
    */
    void processBusinessEvent(const QnAbstractBusinessEventPtr& bEvent);

    /*
    * Execute business action.
    * This function is called if business event already matched to action(s).
    */
    void executeAction(const QnAbstractBusinessActionPtr& action);

protected:
    virtual void prepareAdditionActionParams(const QnAbstractBusinessActionPtr& action) = 0;
protected slots:
    /*
    * Execute action physically. Return true if action success executed
    */
    virtual bool executeActionInternal(const QnAbstractBusinessActionPtr& action);
private slots:
    void at_broadcastBusinessActionFinished(int handle, ec2::ErrorCode errorCode);
    void at_actionDelivered(const QnAbstractBusinessActionPtr& action);
    void at_actionDeliveryFailed(const QnAbstractBusinessActionPtr& action);

    void at_eventRuleChanged(const QnBusinessEventRulePtr& bRule);
    void at_eventRuleRemoved(QnUuid id);
    void at_eventRulesReset(const QnBusinessEventRuleList& rules);

    void toggleInputPortMonitoring(const QnResourcePtr& resource, bool toggle);

    void at_timer();


protected:
    bool containResource(const QnResourceList& resList, const QnUuid& resId) const;
    QnAbstractBusinessActionList matchActions(const QnAbstractBusinessEventPtr& bEvent);

    /*
    * Some actions can be executed on server only. In this case, function returns server where action must be executed
    */
    QnMediaServerResourcePtr getDestMServer(const QnAbstractBusinessActionPtr& action, const QnResourcePtr& res);

    void terminateRunningRule(const QnBusinessEventRulePtr& rule);

private:
    void at_eventRuleChanged_i(const QnBusinessEventRulePtr& bRule);

    QnAbstractBusinessActionPtr processToggleAction(const QnAbstractBusinessEventPtr& bEvent, const QnBusinessEventRulePtr& rule);
    QnAbstractBusinessActionPtr processInstantAction(const QnAbstractBusinessEventPtr& bEvent, const QnBusinessEventRulePtr& rule);
    bool checkRuleCondition(const QnAbstractBusinessEventPtr& bEvent, const QnBusinessEventRulePtr& rule) const;
    bool needProxyAction(const QnAbstractBusinessActionPtr& action, const QnResourcePtr& res);
    void doProxyAction(const QnAbstractBusinessActionPtr& action, const QnResourcePtr& res);
    void executeAction(const QnAbstractBusinessActionPtr& action, const QnResourcePtr& res);

    bool handleBookmarkAction(const QnAbstractBusinessActionPtr& action);

protected:
    mutable QnMutex m_mutex;
private:
    QList<QnBusinessEventRulePtr> m_rules;
    static QnBusinessRuleProcessor* m_instance;

    struct RunningRuleInfo
    {
        RunningRuleInfo() {}
        QMap<QnUuid, QnAbstractBusinessEventPtr> resources;
        QSet<QnUuid> isActionRunning; // actions that has been started by resource. Continues action starts only onces for all event resources.
    };
    typedef QMap<QString, RunningRuleInfo> RunningRuleMap;

    /**
     * @brief m_eventsInProgress         Stores events that are toggled and state is On
     */
    RunningRuleMap m_rulesInProgress;


    /**
     * @brief match resources between event and rule.
     * @return false if business rule isn't match to a source event
     */
    bool checkEventCondition(const QnAbstractBusinessEventPtr& bEvent, const QnBusinessEventRulePtr& rule);

    QMap<QString, QnProcessorAggregationInfo> m_aggregateActions; // aggregation counter for instant actions
    QMap<QString, QSet<QnUuid>> m_actionInProgress;              // remove duplicates for long actions
    QTimer m_timer;

    /*!
        \param isRuleAdded \a true - rule added, \a false - removed
    */
    void notifyResourcesAboutEventIfNeccessary( const QnBusinessEventRulePtr& businessRule, bool isRuleAdded );

    QHash<QnUuid, qint64> m_runningBookmarkActions;
};

#define qnBusinessRuleProcessor QnBusinessRuleProcessor::instance()

#endif // __BUSINESS_RULE_PROCESSOR_H_
