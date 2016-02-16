
#include "actions_statistics_module.h"

#include <ui/actions/action.h>
#include <ui/actions/action_manager.h>
#include <ui/statistics/modules/private/action_metrics.h>

namespace
{
    template<typename MetricsType>
    QSharedPointer<MetricsType> createMetrics(QnActionManager *manager)
    {
        return QSharedPointer<MetricsType>(new MetricsType(manager));
    }
}

QnActionsStatisticsModule::QnActionsStatisticsModule(QObject *parent)
    : base_type(parent)
    , m_actionManager()
    , m_multiMetrics()
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
        m_multiMetrics.clear();
    }

    m_actionManager = manager;

    m_multiMetrics.append(createMetrics<ActionsTriggeredCountMetrics>(manager));
    m_multiMetrics.append(createMetrics<ActionCheckedTimeMetric>(manager));
}

QnMetricsHash QnActionsStatisticsModule::metrics() const
{
    QnMetricsHash result;
    for (const auto multiMetric: m_multiMetrics)
        result.unite(multiMetric->metrics());

    return result;
}

void QnActionsStatisticsModule::resetMetrics()
{
    for (const auto &multiMetrics:m_multiMetrics)
        multiMetrics->reset();
}
