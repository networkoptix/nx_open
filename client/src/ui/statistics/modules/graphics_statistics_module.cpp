
#include "graphics_statistics_module.h"

#include <ui/workbench/workbench_context.h>

#include <statistics/base/metrics_container.h>
#include <ui/statistics/modules/private/avg_tabs_count_metric.h>
#include <ui/statistics/modules/private/camera_fullscreen_metric.h>
#include <ui/statistics/modules/private/preview_search_duration_metric.h>
#include <ui/statistics/modules/private/motion_search_duration_metric.h>
#include <ui/statistics/modules/private/version_metric.h>

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

QnStatisticValuesHash QnGraphicsStatisticsModule::values() const
{
    return (m_metrics ? m_metrics->values() : QnStatisticValuesHash());
}

void QnGraphicsStatisticsModule::reset()
{
    if (m_metrics)
        m_metrics->reset();
}

void QnGraphicsStatisticsModule::recreateMetrics()
{
    m_metrics.reset(new QnMetricsContainer());

    if (m_context)
    {
        const auto workbench = m_context->workbench();
        if (workbench)
        {
            const auto avgTabsCount = QnAbstractMetricPtr(
                new AvgTabsCountMetric(workbench));
            const auto psearchDuration = QnAbstractMetricPtr(
                new PreviewSearchDurationMetric(workbench));

            m_metrics->addMetric(lit("avg_tabs_cnt"), avgTabsCount);
            m_metrics->addMetric(lit("psearch_duration_ms"), psearchDuration);
        }

        const auto display = m_context->display();
        if (display)
        {
            const auto cameraFullscreenMetric = QnAbstractMetricPtr(
                new CameraFullscreenMetric(display));
            m_metrics->addMetric(lit("camera_fullscreen_duration_ms"), cameraFullscreenMetric);
        }

        const auto msearchDuration = QnAbstractMetricPtr(
            new MotionSearchDurationMetric(m_context));
        m_metrics->addMetric(lit("msearch_duration_ms"), msearchDuration);
    }

    const auto versionMetric = QnAbstractMetricPtr(
        new VersionMetric());
    m_metrics->addMetric(lit("version"), versionMetric);
}

