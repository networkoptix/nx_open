
#include "graphics_statistics_module.h"

#include <utils/common/app_info.h>

#include <statistics/base/functor_metric.h>
#include <statistics/base/metrics_container.h>
#include <ui/graphics/opengl/gl_functions.h>
#include <ui/workbench/workbench_context.h>
#include <ui/statistics/modules/private/avg_tabs_count_metric.h>
#include <ui/statistics/modules/private/camera_fullscreen_metric.h>
#include <ui/statistics/modules/private/preview_search_duration_metric.h>
#include <ui/statistics/modules/private/motion_search_duration_metric.h>

QnGraphicsStatisticsModule::QnGraphicsStatisticsModule(QObject *parent)
    : base_type(parent)
    , QnWorkbenchContextAware(parent)
    , m_metrics(new QnMetricsContainer())
{
    const auto avgTabsCount = QnAbstractMetricPtr(
        new AvgTabsCountMetric(context()->workbench()));

    const auto psearchDuration = QnAbstractMetricPtr(
        new PreviewSearchDurationMetric(context()->workbench()));

    const auto cameraFullscreenMetric = QnAbstractMetricPtr(
        new CameraFullscreenMetric(context()->display()));

    const auto msearchDuration = QnAbstractMetricPtr(
        new MotionSearchDurationMetric(context()));

    const auto versionMetric = QnFunctorMetric::create([]()
    {
        return QnAppInfo::applicationVersion();
    });

    const auto archMetric = QnFunctorMetric::create([]()
    {
        return QnAppInfo::applicationArch();
    });

    const auto platformMetric = QnFunctorMetric::create([]()
    {
        return QnAppInfo::applicationPlatform();
    });

    const auto platformModificationMetric = QnFunctorMetric::create([]()
    {
        return QnAppInfo::applicationPlatformModification();
    });

    const auto revisionMetric = QnFunctorMetric::create([]()
    {
        return QnAppInfo::applicationRevision();
    });

    const auto customizationMetric = QnFunctorMetric::create([]()
    {
        return QnAppInfo::customizationName();
    });

    const auto glVersionMetric = QnFunctorMetric::create([]()
    {
        return QnGlFunctions::openGLCachedInfo().version;
    });

    const auto glRendererMetric = QnFunctorMetric::create([]()
    {
        return QnGlFunctions::openGLCachedInfo().renderer;
    });

    const auto glVendorMetric = QnFunctorMetric::create([]()
    {
        return QnGlFunctions::openGLCachedInfo().vendor;
    });

    m_metrics->addMetric(lit("avg_tabs_cnt"), avgTabsCount);
    m_metrics->addMetric(lit("psearch_duration_ms"), psearchDuration);
    m_metrics->addMetric(lit("camera_fullscreen_duration_ms"), cameraFullscreenMetric);
    m_metrics->addMetric(lit("msearch_duration_ms"), msearchDuration);
    m_metrics->addMetric(lit("version"), versionMetric);
    m_metrics->addMetric(lit("arch"), archMetric);
    m_metrics->addMetric(lit("platform"), platformMetric);
    m_metrics->addMetric(lit("platform_modification"), platformModificationMetric);
    m_metrics->addMetric(lit("revision"), revisionMetric);
    m_metrics->addMetric(lit("customization"), customizationMetric);
    m_metrics->addMetric(lit("gl_version"), glVersionMetric);
    m_metrics->addMetric(lit("gl_renderer"), glRendererMetric);
    m_metrics->addMetric(lit("gl_vendor"), glVendorMetric);
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

