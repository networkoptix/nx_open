#include "motion_search_widget.h"

#include <QtWidgets/QAction>

#include <core/resource/camera_resource.h>
#include <ui/style/skin.h>
#include <ui/workbench/workbench_navigator.h>
#include <utils/common/event_processors.h>

#include <nx/client/desktop/event_search/models/motion_search_list_model.h>
#include <nx/utils/log/assert.h>

namespace nx::client::desktop {

namespace {

MotionSearchListModel* motionModel(MotionSearchWidget* widget)
{
    const auto result = qobject_cast<MotionSearchListModel*>(widget->model());
    NX_CRITICAL(result);
    return result;
}

} // namespace

MotionSearchWidget::MotionSearchWidget(QnWorkbenchContext* context, QWidget* parent):
    base_type(context, new MotionSearchListModel(context), parent)
{
    setRelevantControls(Control::cameraSelector | Control::timeSelector | Control::areaSelector
        | Control::previewsToggler);
    setPlaceholderPixmap(qnSkin->pixmap("events/placeholders/motion.png"));

    installEventHandler(this, {QEvent::Show, QEvent::Hide},
        this, &MotionSearchWidget::updateTimelineDisplay);

    connect(this, &AbstractSearchWidget::cameraSetChanged,
        this, &MotionSearchWidget::updateTimelineDisplay);

    connect(navigator(), &QnWorkbenchNavigator::currentResourceChanged,
        this, &MotionSearchWidget::updateTimelineDisplay);

    connect(this, &AbstractSearchWidget::selectedAreaChanged, this,
        [this](bool wholeArea)
        {
            if (wholeArea)
                setFilterArea({});
        });
}

QString MotionSearchWidget::placeholderText(bool constrained) const
{
    return constrained ? tr("No motion") : tr("No motion detected");
}

QString MotionSearchWidget::itemCounterText(int count) const
{
    return tr("%n motion events", "", count);
}

void MotionSearchWidget::setFilterArea(const QList<QRegion>& value)
{
    setWholeArea(std::all_of(value.cbegin(), value.cend(),
        [](const QRegion& r) { return r.isEmpty(); }));

    //motionModel(this)->setFilterArea(value); // TODO: FIXME: #vkutin Implement me!
}

void MotionSearchWidget::updateTimelineDisplay()
{
    if (!isVisible())
    {
        if (navigator()->selectedExtraContent() == Qn::MotionContent)
            navigator()->setSelectedExtraContent(Qn::RecordingContent /*means none*/);
        return;
    }

    const auto currentCamera = navigator()->currentResource()
        .dynamicCast<QnVirtualCameraResource>();

    const bool relevant = currentCamera && cameras().contains(currentCamera);
    navigator()->setSelectedExtraContent(relevant ? Qn::MotionContent : Qn::RecordingContent);
}

} // namespace nx::client::desktop
