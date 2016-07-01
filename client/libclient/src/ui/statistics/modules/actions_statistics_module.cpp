
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
    , m_modules()
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
        m_modules.clear();
    }

    m_actionManager = manager;

    m_modules.append(createMetrics<ActionsTriggeredCountMetrics>(manager));
    m_modules.append(createMetrics<ActionCheckedTimeMetric>(manager));
}

QnStatisticValuesHash QnActionsStatisticsModule::values() const
{
    QnStatisticValuesHash result;
    for (const auto module: m_modules)
        result.unite(module->values());

    return result;
}

void QnActionsStatisticsModule::reset()
{
    for (const auto &module:m_modules)
        module->reset();
}
