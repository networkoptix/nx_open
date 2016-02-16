
#include "durations_statistics_module.h"

#include <ui/statistics/modules/module_private/single_metrics_holder.h>

#include <ui/statistics/modules/module_private/session_uptime_metric.h>
#include <ui/statistics/modules/module_private/app_active_time_metric.h>

QnDurationStatisticsModule::QnDurationStatisticsModule(QObject *parent)
    : base_type(parent)
    , m_metrics(new SingleMetricsHolder())
{
    m_metrics->addMetric<SessionUptimeMetric>(lit("session_ms"));
    m_metrics->addMetric<AppActiveTimeMetric>(lit("active_ms"));
}

QnDurationStatisticsModule::~QnDurationStatisticsModule()
{}

QnMetricsHash QnDurationStatisticsModule::metrics() const
{
    return m_metrics->metrics();
}

void QnDurationStatisticsModule::resetMetrics()
{
    m_metrics->reset();
}
