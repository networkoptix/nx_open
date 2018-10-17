#include "analytics_search_widget.h"

#include <core/resource/camera_resource.h>
#include <ui/style/skin.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_navigator.h>
#include <utils/common/event_processors.h>

#include <nx/client/desktop/event_search/models/analytics_search_list_model.h>
#include <nx/utils/log/assert.h>

namespace nx::client::desktop {

namespace {

AnalyticsSearchListModel* analyticsModel(AnalyticsSearchWidget* widget)
{
    const auto result = qobject_cast<AnalyticsSearchListModel*>(widget->model());
    NX_CRITICAL(result);
    return result;
}

} // namespace

AnalyticsSearchWidget::AnalyticsSearchWidget(QnWorkbenchContext* context, QWidget* parent):
    base_type(context, new AnalyticsSearchListModel(context), parent)
{
    setRelevantControls(Control::defaults | Control::areaSelector | Control::footersToggler);
    setPlaceholderPixmap(qnSkin->pixmap("events/placeholders/analytics.png"));

    installEventHandler(this, {QEvent::Show, QEvent::Hide},
        this, &AnalyticsSearchWidget::updateTimelineDisplay);

    connect(this, &AbstractSearchWidget::cameraSetChanged,
        this, &AnalyticsSearchWidget::updateTimelineDisplay);

    connect(navigator(), &QnWorkbenchNavigator::currentResourceChanged,
        this, &AnalyticsSearchWidget::updateTimelineDisplay);

    connect(this, &AbstractSearchWidget::textFilterChanged,
        [this](const QString& text)
        {
            analyticsModel(this)->setFilterText(text);
            updateTimelineDisplay();
        });

    connect(this, &AbstractSearchWidget::selectedAreaChanged, this,
        [this](bool wholeArea)
        {
            if (wholeArea)
                setFilterRect({});
        });
}

void AnalyticsSearchWidget::setFilterRect(const QRectF& value)
{
    analyticsModel(this)->setFilterRect(value);
    setWholeArea(value.isEmpty());
    updateTimelineDisplay();
}

QString AnalyticsSearchWidget::placeholderText(bool constrained) const
{
    return constrained ? tr("No objects") : tr("No objects detected");
}

QString AnalyticsSearchWidget::itemCounterText(int count) const
{
    return tr("%n objects", "", count);
}

void AnalyticsSearchWidget::updateTimelineDisplay()
{
    if (!isVisible())
    {
        if (navigator()->selectedExtraContent() == Qn::AnalyticsContent)
            navigator()->setSelectedExtraContent(Qn::RecordingContent /*means none*/);
        return;
    }

    const auto currentCamera = navigator()->currentResource()
        .dynamicCast<QnVirtualCameraResource>();

    const bool relevant = currentCamera && cameras().contains(currentCamera);
    if (!relevant)
    {
        navigator()->setSelectedExtraContent(Qn::RecordingContent /*means none*/);
        return;
    }

    analytics::storage::Filter filter;
    filter.deviceIds = {currentCamera->getId()};
    filter.boundingBox = analyticsModel(this)->filterRect();
    filter.freeText = textFilter();
    navigator()->setAnalyticsFilter(filter);
    navigator()->setSelectedExtraContent(Qn::AnalyticsContent);
}

} // namespace nx::client::desktop
