
#include "action_metrics.h"

#include <ui/actions/action.h>
#include <ui/actions/actions.h>
#include <ui/actions/action_manager.h>
#include <ui/statistics/modules/module_private/time_duration_metric.h>

#include <utils/common/model_functions.h>

namespace
{
    template<typename Type>
    QPointer<Type> makePointer(Type *value)
    {
        return QPointer<Type>(value);
    }

    QString aliasByActionId(QnActions::IDType id)
    {
        return QnLexical::serialized(id);
    }

    QString makeAlias(const QString &alias, const QString &postfix)
    {
        return lit("%1_%2").arg(alias, postfix);
    };

    QString makePath(const QString &baseTag
        , const QObject *source)
    {
        if (!source)
            return baseTag;

        QStringList tagsList(baseTag);

        const auto &name = source->objectName();
        if (!name.isEmpty())
            tagsList.append(name);

        const auto className = QString::fromLatin1(source->staticMetaObject.className());
        if (!className.isEmpty() && (className != QLatin1String("QObject")))
            tagsList.append(className);

        return tagsList.join(L'_');
    }
}

//
AbstractActionsMetrics::AbstractActionsMetrics(QnActionManager *actionManager)
    : base_type()
    , AbstractMultimetric()
{
    if (!actionManager)
        return;

    const auto guard = makePointer(this);
    const auto actionManagerPtr = makePointer(actionManager);

    const auto addAction =
        [this, guard, actionManagerPtr](QnActions::IDType id)
    {
        if (!guard || !actionManagerPtr)
            return;

        const auto action = actionManagerPtr->action(id);
        if (!action)
            return;

        addActionMetric(action);
    };

    connect(actionManager, &QnActionManager::actionRegistered, this, addAction);
}

AbstractActionsMetrics::~AbstractActionsMetrics()
{}

void AbstractActionsMetrics::addActionMetric(QnAction *action)
{
    Q_ASSERT_X(false, Q_FUNC_INFO, "Pure virtual function called!");
}

//

ActionsTriggeredCountMetrics::ActionsTriggeredCountMetrics(QnActionManager *actionManager)
    : base_type(actionManager)
    , m_values()
{
    if (!actionManager)
        return;

    for (const auto action: actionManager->actions())
        addActionMetric(action);
}

void ActionsTriggeredCountMetrics::addActionMetric(QnAction *action)
{
    const auto id = action->id();
    const auto guard = makePointer(this);
    const auto processTriggered = [this, guard, id]()
    {
        if (!guard)
            return;

        const auto kTriggerdPostfix = lit("trg");
        const QString path = makePath(kTriggerdPostfix, sender());

        auto& countByParams = m_values[id];
        ++countByParams[kTriggerdPostfix];  // Counts base trigger event number
        ++countByParams[path];
    };

    connect(action, &QAction::triggered, this, processTriggered);
    processTriggered();
}


QnMetricsHash ActionsTriggeredCountMetrics::metrics() const
{
    QnMetricsHash result;
    for (auto it = m_values.cbegin(); it != m_values.end(); ++it)
    {
        const auto actionId = static_cast<QnActions::IDType>(it.key());
        const auto alias = aliasByActionId(actionId);

        const auto &countByParams = it.value();
        for (auto itParams = countByParams.cbegin();
            itParams != countByParams.cend(); ++itParams)
        {
            const auto postfix = itParams.key();
            const auto triggeredCount = itParams.value();
            const auto finalAlias = makeAlias(alias, postfix);
            result.insert(finalAlias, QString::number(triggeredCount));
        }
    }
    return result;
}

void ActionsTriggeredCountMetrics::reset()
{
    m_values.clear();
}

//

ActionCheckedTimeMetric::ActionCheckedTimeMetric(QnActionManager *actionManager)
    : base_type(actionManager)
    , m_metrics()
{
    if (!actionManager)
        return;

    for (const auto action: actionManager->actions())
        addActionMetric(action);
}

ActionCheckedTimeMetric::~ActionCheckedTimeMetric()
{}

void ActionCheckedTimeMetric::addActionMetric(QnAction *action)
{
    const auto id = action->id();
    const auto guard = makePointer(this);
    const auto processToggled = [this, guard, id](bool checked)
    {
        if (!guard)
            return;

        auto it = m_metrics.find(id);
        if (it == m_metrics.end())
            it = m_metrics.insert(id, TimeDurationMetricPtr(new TimeDurationMetric()));

        const auto &timeDurationMetric = *it;
        if (timeDurationMetric->isActive() == checked)
            return;

        timeDurationMetric->activateCounter(checked);
    };
    connect(action, &QAction::toggled, this, processToggled);
    processToggled(action->isChecked());
}


QnMetricsHash ActionCheckedTimeMetric::metrics() const
{
    QnMetricsHash result;
    for (auto it = m_metrics.begin(); it != m_metrics.end(); ++it)
    {
        const auto metric = it.value();
        if (metric->duration() <= 0)
            continue;

        static const auto kCheckedTimePostfix = lit("chkd_ms");

        const auto actionId = static_cast<QnActions::IDType>(it.key());
        const auto alias = aliasByActionId(actionId);
        const auto finalAlias = makeAlias(alias, kCheckedTimePostfix);
        result.insert(finalAlias, metric->value());
    }
    return result;
}

void ActionCheckedTimeMetric::reset()
{
    for (const auto metric: m_metrics)
        metric->reset();
}