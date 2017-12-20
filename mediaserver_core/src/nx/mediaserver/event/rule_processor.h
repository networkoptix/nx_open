
#pragma once

#include <QtCore/QTimer>
#include <QtCore/QThread>
#include <QtCore/QMultiMap>
#include <QElapsedTimer>

#include <common/common_module_aware.h>
#include <core/resource/resource_fwd.h>
#include <utils/common/request_param.h>
#include <nx_ec/ec_api.h>
#include <nx/utils/singleton.h>
#include <nx/utils/thread/mutex.h>
#include <nx/mediaserver/event/event_message_bus.h>
#include <nx/vms/event/aggregation_info.h>
#include <nx/vms/event/events/abstract_event.h>
#include <nx/vms/event/actions/abstract_action.h>
#include <nx/vms/event/rule.h>

class EmailManagerImpl;

namespace nx {
namespace mediaserver {
namespace event {

class ProcessorAggregationInfo
{
public:
    ProcessorAggregationInfo(): m_initialized(false)
    {
        m_timer.invalidate();
    }

    /** Timestamp when the action should be executed. */
    bool isExpired() const
    {
        return m_timer.isValid()
            ? m_timer.hasExpired(m_rule->aggregationPeriod() * 1000ll)
            : true;
    }

    void init(const vms::event::AbstractEventPtr& event, const vms::event::RulePtr& rule)
    {
        m_event = event;
        m_rule = rule;
        m_initialized = true;
    }

    /** Restores the initial state. */
    void reset()
    {
        m_timer.restart();
        m_info.clear();
        m_initialized = false;
    }

    void append(const vms::event::EventParameters& runtimeParams)
    {
        m_info.append(runtimeParams);
    }

    bool initialized() const { return m_initialized; }

    int totalCount() const { return m_info.totalCount(); }

    vms::event::AbstractEventPtr event() const { return m_event; }

    vms::event::RulePtr rule() const { return m_rule; }

    const vms::event::AggregationInfo& info() const { return m_info; }

private:
    vms::event::AbstractEventPtr m_event;
    vms::event::RulePtr m_rule;
    vms::event::AggregationInfo m_info;

    /** Agregation timer */
    QElapsedTimer m_timer;

    /** Flag that event and rule are set. */
    bool m_initialized;
};

/*
* This class route business event and generate business action
*/
class RuleProcessor:
    public QThread,
    public Singleton<RuleProcessor>,
    public QnCommonModuleAware
{
    Q_OBJECT

public:
    RuleProcessor(QnCommonModule* commonModule);
    virtual ~RuleProcessor() override;

    void addRule(const vms::event::RulePtr& value);

    /*
    * Return module GUID.
    * If destination action intended for current module,
    * no route through message bus is required.
    */
    virtual QnUuid getGuid() const { return QnUuid(); }

    bool broadcastAction(const vms::event::AbstractActionPtr& action);

    /*
     * Get pointer to rule, that catched provided action
     */
    vms::event::RulePtr getRuleForAction(const vms::event::AbstractActionPtr& action) const;
public slots:
    /*
    * This function matches all business actions for specified business event and execute it.
    * So, call this function if business event occurred.
    */
    void processEvent(const vms::event::AbstractEventPtr& event);

    /*
    * Execute business action.
    * This function is called if business event already matched to action(s).
    */
    void executeAction(const vms::event::AbstractActionPtr& action);

protected:
    virtual void prepareAdditionActionParams(const vms::event::AbstractActionPtr& action) = 0;

protected slots:
    /*
    * Execute action physically. Return true if action success executed
    */
    virtual bool executeActionInternal(const vms::event::AbstractActionPtr& action);

private slots:
    void at_broadcastActionFinished(int handle, ec2::ErrorCode errorCode);
    void at_actionDelivered(const vms::event::AbstractActionPtr& action);
    void at_actionDeliveryFailed(const vms::event::AbstractActionPtr& action);

    void at_ruleAddedOrUpdated(const vms::event::RulePtr& rule);
    void at_ruleRemoved(QnUuid id);
    void at_rulesReset(const vms::event::RuleList& rules);

    void toggleInputPortMonitoring(const QnResourcePtr& resource, bool on);

    void at_timer();

protected:
    bool containsResource(const QnResourceList& resList, const QnUuid& resId) const;

    typedef std::list<std::pair<vms::event::RulePtr, vms::event::AbstractActionPtr> > EventActions;
    EventActions matchActions(const vms::event::AbstractEventPtr& event);

    /*
    * Some actions can be executed on server only.
    * In this case, function returns server where action must be executed.
    */
    QnMediaServerResourcePtr getDestinationServer(const vms::event::AbstractActionPtr& action,
        const QnResourcePtr& res);

    void terminateRunningRule(const vms::event::RulePtr& rule);

    bool fixActionTimeFields(const vms::event::AbstractActionPtr& action);

    // Check if we should omit this action from mentioning in DB
    // @param action - action to check
    // @param rule - rule that triggered this action
    bool shouldOmitActionLogging(const vms::event::AbstractActionPtr& action) const;

private:
    void at_ruleAddedOrUpdated_impl(const vms::event::RulePtr& rule);

    vms::event::AbstractActionPtr processToggleableAction(
        const vms::event::AbstractEventPtr& event, const vms::event::RulePtr& rule);

    vms::event::AbstractActionPtr processInstantAction(
        const vms::event::AbstractEventPtr& event, const vms::event::RulePtr& rule);

    bool checkRuleCondition(const vms::event::AbstractEventPtr& event,
        const vms::event::RulePtr& rule) const;

    bool needProxyAction(const vms::event::AbstractActionPtr& action, const QnResourcePtr& res);
    void doProxyAction(const vms::event::AbstractActionPtr& action, const QnResourcePtr& res);
    void doExecuteAction(const vms::event::AbstractActionPtr& action, const QnResourcePtr& res);

    bool updateProlongedActionStartTime(const vms::event::AbstractActionPtr& action);
    bool popProlongedActionStartTime(
        const vms::event::AbstractActionPtr& action,
        qint64& startTimeUsec);

protected:
    mutable QnMutex m_mutex;

private:
    QList<vms::event::RulePtr> m_rules;
    static RuleProcessor* m_instance;

    struct RunningRuleInfo
    {
        RunningRuleInfo() {}
        QMap<QnUuid, vms::event::AbstractEventPtr> resources;
        QSet<QnUuid> isActionRunning; // actions that has been started by resource. Continues action starts only onces for all event resources.
    };

    using RunningRuleMap = QMap<QString, RunningRuleInfo>;

    /**
     * @brief m_eventsInProgress Events that are toggled and state is On
     */
    RunningRuleMap m_rulesInProgress;

    /**
     * @brief match resources between event and rule.
     * @return false if business rule isn't match to a source event
     */
    bool checkEventCondition(const vms::event::AbstractEventPtr& event, const vms::event::RulePtr& rule) const;

    QMap<QString, ProcessorAggregationInfo> m_aggregateActions; // aggregation counter for instant actions
    QMap<QString, QSet<QnUuid>> m_actionInProgress;               // remove duplicates for long actions
    QTimer m_timer;

    /*!
        \param isRuleAdded \a true - rule added, \a false - removed
    */
    void notifyResourcesAboutEventIfNeccessary(const vms::event::RulePtr& rule, bool isRuleAdded);

    QHash<QnUuid, qint64> m_runningBookmarkActions;
};

#define qnEventRuleProcessor nx::mediaserver::event::RuleProcessor::instance()

} // namespace event
} // namespace mediaserver
} // namespace nx
