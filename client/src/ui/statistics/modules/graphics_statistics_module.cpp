
#include "graphics_statistics_module.h"

#include <ui/workbench/workbench_context.h>

#include <ui/statistics/modules/private/single_metrics_holder.h>
#include <ui/statistics/modules/private/avg_tabs_count_metric.h>

QnGraphicsStatisticsModule::QnGraphicsStatisticsModule(QObject *parent)
    : base_type(parent)
    , m_context()
    , m_metrics()
{}

QnGraphicsStatisticsModule::~QnGraphicsStatisticsModule()
{}

void QnGraphicsStatisticsModule::setContext(QnWorkbenchContext *context)
{
    if (m_context == context)
        return;

    if (m_context)
        disconnect(m_context, nullptr, this, nullptr);

    m_context = context;

    recreateMetrics();
}

QnMetricsHash QnGraphicsStatisticsModule::metrics() const
{
    return (m_metrics ? m_metrics->metrics() : QnMetricsHash());
}

void QnGraphicsStatisticsModule::resetMetrics()
{
    if (m_metrics)
        m_metrics->reset();
}

void QnGraphicsStatisticsModule::recreateMetrics()
{
    m_metrics.reset(new SingleMetricsHolder());

    if (m_context)
    {
        m_metrics->addMetric(lit("avg_tabs_cnt"), AbstractSingleMetricPtr(
            new AvgTabsCountMetric(m_context->workbench())));
    }
}

