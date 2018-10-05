#include "analytics_search_widget.h"

#include <core/resource/camera_resource.h>
#include <ui/style/skin.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_navigator.h>
#include <utils/common/event_processors.h>

#include <nx/client/desktop/event_search/models/analytics_search_list_model.h>
#include <nx/utils/log/assert.h>

namespace nx::client::desktop {

AnalyticsSearchWidget::AnalyticsSearchWidget(QnWorkbenchContext* context, QWidget* parent):
    base_type(context, new AnalyticsSearchListModel(context), parent)
{
    setRelevantControls(Control::defaults | Control::areaSelector | Control::footersToggler);
    setPlaceholderPixmap(qnSkin->pixmap("events/placeholders/analytics.png"));

    const auto updateChunksFilter =
        [this]()
        {
            if (!isVisible())
            {
                if (navigator()->selectedExtraContent() == Qn::AnalyticsContent)
                    navigator()->setSelectedExtraContent(Qn::RecordingContent /*means none*/);
                return;
            }

            const auto cameras = this->cameras();
            const auto currentCamera = navigator()->currentResource()
                .dynamicCast<QnVirtualCameraResource>();

            const bool relevant = cameras.empty() || cameras.contains(currentCamera);
            if (!relevant)
            {
                navigator()->setSelectedExtraContent(Qn::RecordingContent /*means none*/);
                return;
            }

            analytics::storage::Filter filter;
            filter.deviceIds = {currentCamera->getId()};
            filter.boundingBox = selectedArea();
            filter.freeText = textFilter();
            navigator()->setAnalyticsFilter(filter);
            navigator()->setSelectedExtraContent(Qn::AnalyticsContent);
        };

    installEventHandler(this, {QEvent::Show, QEvent::Hide}, this, updateChunksFilter);

    connect(this, &AbstractSearchWidget::cameraSetChanged, updateChunksFilter);
    connect(this, &AbstractSearchWidget::selectedAreaChanged, updateChunksFilter);

    connect(navigator(), &QnWorkbenchNavigator::currentResourceChanged,
        this, updateChunksFilter);

    connect(this, &AbstractSearchWidget::textFilterChanged,
        [this, updateChunksFilter](const QString& text)
        {
            auto analyticsModel = qobject_cast<AnalyticsSearchListModel*>(model());
            NX_CRITICAL(analyticsModel);
            analyticsModel->setFilterText(text);
            updateChunksFilter();
        });

    connect(this, &AbstractSearchWidget::selectedAreaChanged,
        [this, updateChunksFilter](const QRectF& area)
        {
            auto analyticsModel = qobject_cast<AnalyticsSearchListModel*>(model());
            NX_CRITICAL(analyticsModel);
            analyticsModel->setFilterRect(area);
            updateChunksFilter();
        });
}

QString AnalyticsSearchWidget::placeholderText(bool constrained) const
{
    return constrained ? tr("No objects") : tr("No objects detected");
}

QString AnalyticsSearchWidget::itemCounterText(int count) const
{
    return tr("%n objects", "", count);
}

} // namespace nx::client::desktop
