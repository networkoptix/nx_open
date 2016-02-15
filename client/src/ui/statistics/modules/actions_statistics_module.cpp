
#include "actions_statistics_module.h"

#include <ui/actions/action.h>
#include <ui/actions/action_manager.h>
#include <ui/statistics/modules/actions_module_private/action_metrics.h>

#include <utils/common/model_functions.h>

namespace
{
    QString aliasByActionId(QnActions::Type id)
    {
        return QnLexical::serialized(id);
    }

    QString makeAlias(const QString &alias, const QString &postfix)
    {
        return lit("%1_%2").arg(alias, postfix);
    };
}

QnActionsStatisticsModule::QnActionsStatisticsModule(QObject *parent)
    : base_type(parent)
    , m_actionManager()
{
}

QnActionsStatisticsModule::~QnActionsStatisticsModule()
{
}

void QnActionsStatisticsModule::setActionManager(const QnActionManagerPtr &manager)
{
    if (m_actionManager == manager)
        return;

    if (m_actionManager)
    {
        disconnect(m_actionManager, nullptr, this, nullptr);
        m_metrics.clear();
    }

    m_actionManager = manager;

    QPointer<QnActionsStatisticsModule> guard(this);
    const auto createActionMetrics = [this, guard](QnActions::Type id)
    {
        if (!guard || !m_actionManager)
            return;

        const auto &alias = aliasByActionId(id);

        const auto triggeredAlias = makeAlias(alias, ActionTriggeredCountMetric::kPostfix);
        m_metrics[triggeredAlias] = MetricsPtr(new ActionTriggeredCountMetric(m_actionManager, id));

        // TODO: Add metrics handler
    };

    connect(m_actionManager, &QnActionManager::actionRegistered, this, createActionMetrics);

    for (const auto action: m_actionManager->actions())
        createActionMetrics(action->id());
}

QnMetricsHash QnActionsStatisticsModule::metrics() const
{
    QnMetricsHash result;
    for (auto it = m_metrics.cbegin(); it != m_metrics.cend(); ++it)
    {
        const auto alias = it.key();
        const auto subMetrics = it.value()->metrics();
        for (auto itSub = subMetrics.cbegin(); itSub != subMetrics.cend(); ++itSub)
            result.insert(alias, itSub.value());
    }
    return result;
}

void QnActionsStatisticsModule::resetMetrics()
{
    for (const auto &metric: m_metrics)
        metric->reset();
}
