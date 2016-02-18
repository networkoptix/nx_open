
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
    , QnWorkbenchContextAware(parent)
    , m_metrics(new QnMetricsContainer())
{
    const auto avgTabsCount = QnAbstractMetricPtr(
        new AvgTabsCountMetric(context()->workbench()));
    const auto psearchDuration = QnAbstractMetricPtr(
        new PreviewSearchDurationMetric(context()->workbench()));

    m_metrics->addMetric(lit("avg_tabs_cnt"), avgTabsCount);
    m_metrics->addMetric(lit("psearch_duration_ms"), psearchDuration);

    const auto cameraFullscreenMetric = QnAbstractMetricPtr(
        new CameraFullscreenMetric(context()->display()));
    m_metrics->addMetric(lit("camera_fullscreen_duration_ms"), cameraFullscreenMetric);

    const auto msearchDuration = QnAbstractMetricPtr(
        new MotionSearchDurationMetric(context()));
    m_metrics->addMetric(lit("msearch_duration_ms"), msearchDuration);

    const auto versionMetric = QnAbstractMetricPtr(
        new VersionMetric());
    m_metrics->addMetric(lit("version"), versionMetric);
}

QnGraphicsStatisticsModule::~QnGraphicsStatisticsModule()
{}

QnStatisticValuesHash QnGraphicsStatisticsModule::values() const
{

    return m_metrics->values();
}

void QnGraphicsStatisticsModule::reset()
{
    m_metrics->reset();
}

