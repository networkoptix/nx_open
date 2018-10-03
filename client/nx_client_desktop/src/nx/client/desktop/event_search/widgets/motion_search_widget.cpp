#include "motion_search_widget.h"

#include <ui/style/skin.h>

#include <nx/client/desktop/event_search/models/motion_search_list_model.h>

namespace nx::client::desktop {

MotionSearchWidget::MotionSearchWidget(QnWorkbenchContext* context, QWidget* parent):
    base_type(context, new MotionSearchListModel(context), parent)
{
    setRelevantControls(Control::areaSelector | Control::previewsToggler);
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

} // namespace nx::client::desktop
