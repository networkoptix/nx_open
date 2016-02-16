
#include "durations_statistics_module.h"

#include <ui/statistics/modules/module_private/time_duration_metric.h>

#include <ui/statistics/modules/module_private/durations/session_uptime_metric.h>
#include <ui/statistics/modules/module_private/durations/app_active_time_metric.h>

namespace
{
    template<typename MetricType>
    QSharedPointer<MetricType> makeMetric()
    {
        // TODO: #ynikitenkov Move to future helpers? see other stat modules
        return QSharedPointer<MetricType>(new MetricType());
    }
}

QnDurationStatisticsModule::QnDurationStatisticsModule(QObject *parent)
    : base_type(parent)
{
    m_metrics.insert(lit("session_ms"), makeMetric<SessionUptimeMetric>());
    m_metrics.insert(lit("active_ms"), makeMetric<AppActiveTimeMetric>());
}

QnDurationStatisticsModule::~QnDurationStatisticsModule()
{}

QnMetricsHash QnDurationStatisticsModule::metrics() const
{
    // TODO: #ynikitenkov Move this part to helpers as template?
    QnMetricsHash result;
    for (auto it = m_metrics.begin(); it != m_metrics.end(); ++it)
    {
        const auto alias = it.key();
        const auto metric = it.value();
        result.insert(alias, metric->value());
    }

    return result;
}

void QnDurationStatisticsModule::resetMetrics()
{
    for(const auto metric: m_metrics)
        metric->reset();
}
