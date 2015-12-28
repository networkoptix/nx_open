
#include "actions_statistics_handler.h"

#include <ui/actions/action.h>
#include <ui/actions/action_manager.h>
#include <ui/statistics/abstract_statistics_metric.h>

using namespace statistics;
using namespace statistics::details;

namespace
{
    template<typename MetricValueType>
    class BaseActionMetric : public Connective< QnTypedAbstractStatisticsMetric<MetricValueType> >
    {
        typedef Connective< QnTypedAbstractStatisticsMetric<MetricValueType> > BaseType;
    public:
        BaseActionMetric(const QString &alias
            , QAction *action
            , ActionMetricParams params
            , QnActionManager *actionManager);

        virtual ~BaseActionMetric();

    protected:
        const ActionMetricParams &params() const;

        QAction *action() const;

        bool isValidEmitter() const;

    private:
        QnActionManager * const m_actionManager;
        QAction * const m_action;
        const ActionMetricParams m_params;
    };

    template<typename MetricValueType>
    BaseActionMetric<MetricValueType>::BaseActionMetric(const QString &alias
        , QAction *action
        , ActionMetricParams params
        , QnActionManager *actionManager)
        : BaseType(alias)

        , m_actionManager(actionManager)
        , m_params(params)
        , m_action(action)
    {}

    template<typename MetricValueType>
    BaseActionMetric<MetricValueType>::~BaseActionMetric()
    {}

    template<typename MetricValueType>
    const ActionMetricParams &BaseActionMetric<MetricValueType>::params() const
    {
        return m_params;
    }

    template<typename MetricValueType>
    QAction *BaseActionMetric<MetricValueType>::action() const
    {
        return m_action;
    }

    template<typename MetricValueType>
    bool BaseActionMetric<MetricValueType>::isValidEmitter() const
    {
        if (m_params.filter == kAllEmitters)
            return true;

        const QnActionParameters actionParams = m_actionManager->currentParameters(sender());

        const ActionEmitterType emitterType = actionParams.argument<ActionEmitterType>(Qn::ActionEmitterType, kManual);
        if (!(emitterType & m_params.filter))
            return false;

        if (!(emitterType & kObject))   // We don't need checks below if
            return true;                // there is no emitter object supposed

        static const auto getObjectTypeName = [](const QObject *object)
        {
            return QLatin1String(object->metaObject()->className());
        };

        const QObject *object = actionParams.argument(Qn::ActionEmittedBy, nullptr);
        return (object && (m_params.objectTypeFilter == getObjectTypeName(object)));
    }

    //

    class ActionCounterMetric : public BaseActionMetric<int>
    {
        typedef BaseActionMetric<int> BaseType;

    public:
        ActionCounterMetric(const QString &alias
            , QAction *action
            , ActionMetricParams params
            , QnActionManager *actionManager);

    private:
        void onTiggered();

        void onToggled();
    };

    ActionCounterMetric::ActionCounterMetric(const QString &alias
        , QAction *action
        , ActionMetricParams params
        , QnActionManager *actionManager)

        : BaseType(alias, action, params, actionManager)
    {
        const bool allowedMetricTypes = ((params.type != kOccuranciesCount)
            || (params.type != kActivationsCount)
            || (params.type != kDeactivationsCount));

        Q_ASSERT_X(allowedMetricTypes, Q_FUNC_INFO
            , "Can't create ActionCountMetric with specified parameters");

        if (!allowedMetricTypes)
            return;

        const auto connectDisconnectHandler = [this, action, params]()
        {
            if (!isTurnedOn())
            {
                disconnect(action, nullptr, this, nullptr);
                return;
            }

            if (params.type == kOccuranciesCount)
                connect(action, &QAction::triggered, this, &ActionCounterMetric::onTiggered);
            else
                connect(action, &QAction::toggled, this, &ActionCounterMetric::onToggled);
        };

        connect(this, &QnAbstractStatisticsMetric::turnedOnChanged, this, connectDisconnectHandler);
    }

    void ActionCounterMetric::onTiggered()
    {
        if (!isTurnedOn() || !isValidEmitter())
            return;

        setValue(value() + 1);

        qDebug() << "Statistic values for " << BaseType::alias() << ":" << value();
    }

    void ActionCounterMetric::onToggled()
    {
        if (!isTurnedOn() || !isValidEmitter())
            return;

        const bool isChecked = action()->isChecked();
        const bool isActivationMetric = (params().type == kActivationsCount);
        if (isChecked != isActivationMetric)
            return;

        setValue(value() + 1);
        qDebug() << "Statistic values for " << BaseType::alias() << ":" << value();
    }

    //

    class ActionUptimeMetric : public BaseActionMetric<int>
    {
        typedef BaseActionMetric<int> BaseType;

    public:
        ActionUptimeMetric(const QString &alias
            , QAction *action
            , ActionMetricParams params
            , QnActionManager *actionManager);

        virtual ~ActionUptimeMetric();

    private:
        void stopTimer();

    private:
        QElapsedTimer m_activeStateTimer;
    };

    ActionUptimeMetric::ActionUptimeMetric(const QString &alias
        , QAction *action
        , ActionMetricParams params
        , QnActionManager *actionManager)

        : BaseType(alias, action, params, actionManager)
    {
        Q_ASSERT_X(params.type == kUptime, Q_FUNC_INFO
            , "Can't create ActionUptimeMetric with specified parameters");

        if (params.type != kUptime)
            return;

        const auto connectDisconnectHandler = [this, action, params]()
        {
            if (!isTurnedOn())
            {
                disconnect(action, nullptr, this, nullptr);
                return;
            }

            const auto triggeredHandler = [this, params, action]()
            {
                const bool isChecked = action->isChecked();
                if (!isChecked)
                {
                    stopTimer();    // It does not matter if emitter is valid or not. Anyway
                                    // we should stop action "activity" time measurement
                    return;
                }

                if (!isValidEmitter() || !isTurnedOn())
                    return;

                m_activeStateTimer.start();
            };

            connect(action, &QAction::toggled, this, triggeredHandler);
        };

        connect(this, &QnAbstractStatisticsMetric::turnedOnChanged, this, connectDisconnectHandler);
    }

    ActionUptimeMetric::~ActionUptimeMetric()
    {
        stopTimer();
    }

    void ActionUptimeMetric::stopTimer()
    {
        if (!m_activeStateTimer.isValid())  // Timer has not been started
            return;

        enum { kMillisecondsInSecond = 1000 };
        setValue(value() + m_activeStateTimer.elapsed() / kMillisecondsInSecond);

        qDebug() << alias() << " uptime is " << value();
        m_activeStateTimer.invalidate();
    }
}

//

ActionMetricParams::ActionMetricParams(MetricType initType
    , ActionEmitterType initFilter
    , QLatin1String initObjectTypeFilter)

    : type(initType)
    , filter(initFilter)
    , objectTypeFilter(initObjectTypeFilter)
{
}

//

ActionsStatisticHandler::ActionsStatisticHandler(QObject *parent)
    : BaseType(parent)
    , m_metrics()
{
}

ActionsStatisticHandler::~ActionsStatisticHandler()
{

}

void ActionsStatisticHandler::watchAction(int id
    , const QString &alias
    , const ActionMetricParams &params)
{
    auto actionInstance = action(static_cast<Qn::ActionId>(id));

    // No action with specified id is available
    if (!actionInstance)
        return;

    QnAbstractStatisticsMetricPtr actionMetric;
    switch(params.type)
    {
    case kOccuranciesCount:
    case kActivationsCount:
    case kDeactivationsCount:
        actionMetric.reset(new ActionCounterMetric(alias, actionInstance, params, menu()));
        break;
    case kUptime:
        actionMetric.reset(new ActionUptimeMetric(alias, actionInstance, params, menu()));
        break;
    default:
         return;
    }

    actionMetric->setTurnedOn(true);
    m_metrics.append(actionMetric);
}
