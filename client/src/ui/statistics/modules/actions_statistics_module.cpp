
#include "actions_statistics_module.h"

#include <ui/actions/action.h>
#include <ui/actions/action_manager.h>
#include <ui/statistics/modules/module_private/actions/action_metrics.h>

#include <utils/common/model_functions.h>

namespace
{
    QString aliasByActionId(QnActions::IDType id)
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
    , m_triggeredCountMetrics()
    , m_checkedTimeMetrics()
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
        m_triggeredCountMetrics.clear();
    }

    m_actionManager = manager;

    QPointer<QnActionsStatisticsModule> guard(this);
    const auto createActionMetrics = [this, guard](QnActions::IDType id)
    {
        if (!guard || !m_actionManager)
            return;

        const auto &alias = aliasByActionId(id);

        const auto triggeredAlias = makeAlias(alias
            , ActionTriggeredCountMetric::kPostfix);
        //m_triggeredCountMetrics[triggeredAlias] = MetricsPtr(
        //    new ActionTriggeredCountMetric(m_actionManager, id));

        static const auto kCheckedTimePostfix = lit("chkd_tm_ms");
        const auto checkedTimeAlias = makeAlias(alias, kCheckedTimePostfix);
        const auto checkedTimeMetricMetric = CheckedTimeMetricPtr(
            new ActionCheckedTimeMetric(m_actionManager, id));
        m_checkedTimeMetrics.insert(checkedTimeAlias, checkedTimeMetricMetric);

        // TODO: Add metrics handler ?
    };

    connect(m_actionManager, &QnActionManager::actionRegistered, this, createActionMetrics);

    for (const auto action: m_actionManager->actions())
        createActionMetrics(action->id());
}

QnMetricsHash QnActionsStatisticsModule::metrics() const
{
    QnMetricsHash result;
    for (auto it = m_triggeredCountMetrics.cbegin(); it != m_triggeredCountMetrics.cend(); ++it)
    {
        const auto alias = it.key();
        const auto subMetrics = it.value()->metrics();
        for (auto itSub = subMetrics.cbegin(); itSub != subMetrics.cend(); ++itSub)
            result.insert(alias, itSub.value());
    }

    for (auto it = m_checkedTimeMetrics.cbegin(); it != m_checkedTimeMetrics.cend(); ++it)
    {
        const auto alias = it.key();
        const auto metric = it.value();
        if (metric->duration() != 0)
            result.insert(it.key(), metric->value());
    }

    return result;
}

void QnActionsStatisticsModule::resetMetrics()
{
    for (const auto &metric: m_checkedTimeMetrics)
        metric->reset();
    for (const auto &metric: m_triggeredCountMetrics)
        metric->reset();
}
