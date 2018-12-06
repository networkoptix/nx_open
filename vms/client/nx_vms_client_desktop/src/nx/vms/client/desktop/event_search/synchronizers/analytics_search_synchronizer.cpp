#include "analytics_search_synchronizer.h"

#include <core/resource/camera_resource.h>
#include <ui/graphics/items/resource/media_resource_widget.h>
#include <ui/workbench/workbench_navigator.h>

#include <nx/utils/log/assert.h>
#include <nx/vms/client/desktop/event_search/widgets/analytics_search_widget.h>

namespace nx::vms::client::desktop {

AnalyticsSearchSynchronizer::AnalyticsSearchSynchronizer(
    QnWorkbenchContext* context,
    AnalyticsSearchWidget* analyticsSearchWidget,
    QObject* parent)
    :
    AbstractSearchSynchronizer(context, parent),
    m_analyticsSearchWidget(analyticsSearchWidget)
{
    NX_CRITICAL(m_analyticsSearchWidget);

    connect(this, &AbstractSearchSynchronizer::mediaWidgetAboutToBeChanged, this,
        [this](QnMediaResourceWidget* mediaWidget)
        {
            if (!mediaWidget)
                return;

            mediaWidget->disconnect(this);
            mediaWidget->unsetAreaSelectionType(QnMediaResourceWidget::AreaType::analytics);
        });

    connect(this, &AbstractSearchSynchronizer::mediaWidgetChanged, this,
        [this](QnMediaResourceWidget* mediaWidget)
        {
            if (!mediaWidget)
                return;

            updateAreaSelection();

            connect(mediaWidget, &QnMediaResourceWidget::analyticsFilterRectChanged, this,
                [this]()
                {
                    updateAreaSelection();

                    // Stop selection after selecting once.
                    this->mediaWidget()->setAreaSelectionEnabled(
                        QnMediaResourceWidget::AreaType::analytics, false);
                });
        });

    connect(this, &AbstractSearchSynchronizer::activeChanged, this,
        [this]()
        {
            updateAreaSelection();
            updateTimelineDisplay();
        });

    connect(m_analyticsSearchWidget.data(), &AnalyticsSearchWidget::areaSelectionEnabledChanged,
        this, &AnalyticsSearchSynchronizer::updateAreaSelection);

    connect(m_analyticsSearchWidget.data(), &AnalyticsSearchWidget::areaSelectionRequested, this,
        [this]()
        {
            const auto mediaWidget = this->mediaWidget();
            if (mediaWidget && active())
            {
                mediaWidget->setAreaSelectionEnabled(
                    QnMediaResourceWidget::AreaType::analytics, true);
            }
        });

    connect(m_analyticsSearchWidget.data(), &AnalyticsSearchWidget::cameraSetChanged,
        this, &AnalyticsSearchSynchronizer::updateTimelineDisplay);

    connect(m_analyticsSearchWidget.data(), &AnalyticsSearchWidget::textFilterChanged,
        this, &AnalyticsSearchSynchronizer::updateTimelineDisplay);

    connect(m_analyticsSearchWidget.data(), &AnalyticsSearchWidget::filterRectChanged, this,
        [this](const QRectF& value)
        {
            updateTimelineDisplay();

            const auto mediaWidget = this->mediaWidget();
            if (value.isNull() && mediaWidget && m_analyticsSearchWidget->areaSelectionEnabled())
                mediaWidget->setAnalyticsFilterRect({});
        });

    connect(navigator(), &QnWorkbenchNavigator::currentResourceChanged,
        this, &AnalyticsSearchSynchronizer::updateTimelineDisplay);
}

void AnalyticsSearchSynchronizer::updateAreaSelection()
{
    const auto mediaWidget = this->mediaWidget();
    if (!mediaWidget || !m_analyticsSearchWidget)
        return;

    if (!active())
    {
        mediaWidget->unsetAreaSelectionType(QnMediaResourceWidget::AreaType::analytics);
        return;
    }

    m_analyticsSearchWidget->setFilterRect(m_analyticsSearchWidget->areaSelectionEnabled()
        ? mediaWidget->analyticsFilterRect()
        : QRectF());

    mediaWidget->setAreaSelectionType(m_analyticsSearchWidget->areaSelectionEnabled()
        ? QnMediaResourceWidget::AreaType::analytics
        : QnMediaResourceWidget::AreaType::none);
}

void AnalyticsSearchSynchronizer::updateTimelineDisplay()
{
    if (!active())
    {
        setTimeContentDisplayed(Qn::AnalyticsContent, false);
        return;
    }

    const auto camera = navigator()->currentResource().dynamicCast<QnVirtualCameraResource>();
    const bool relevant = camera && m_analyticsSearchWidget->cameras().contains(camera);
    if (!relevant)
    {
        navigator()->setSelectedExtraContent(Qn::RecordingContent /*means none*/);
        return;
    }

    analytics::storage::Filter filter;
    filter.deviceIds = {camera->getId()};
    filter.boundingBox = m_analyticsSearchWidget->filterRect();
    filter.freeText = m_analyticsSearchWidget->textFilter();
    navigator()->setAnalyticsFilter(filter);
    navigator()->setSelectedExtraContent(Qn::AnalyticsContent);
}

} // namespace nx::vms::client::desktop
