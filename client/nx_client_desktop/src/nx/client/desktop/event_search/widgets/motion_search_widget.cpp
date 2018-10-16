#include "motion_search_widget.h"

#include <QtWidgets/QAction>

#include <ui/style/skin.h>

#include <nx/client/desktop/event_search/models/motion_search_list_model.h>
#include <nx/utils/log/assert.h>

namespace nx::client::desktop {

MotionSearchWidget::MotionSearchWidget(QnWorkbenchContext* context, QWidget* parent):
    base_type(context, new MotionSearchListModel(context), parent)
{
    setRelevantControls(Control::cameraSelector | Control::timeSelector | Control::areaSelector
        | Control::previewsToggler);
    setPlaceholderPixmap(qnSkin->pixmap("events/placeholders/motion.png"));
}

QString MotionSearchWidget::placeholderText(bool constrained) const
{
    return constrained ? tr("No motion") : tr("No motion detected");
}

QString MotionSearchWidget::itemCounterText(int count) const
{
    return tr("%n motion events", "", count);
}

void MotionSearchWidget::setCurrentMotionSearchEnabled(bool value)
{
    const bool currentlyEnabled = selectedCameras() == Cameras::current;
    if (currentlyEnabled == value)
        return;

    const auto action = cameraSelectionAction(value ? Cameras::current : previousCameras());
    NX_ASSERT(action);
    if (action)
        action->trigger();
}

} // namespace nx::client::desktop
