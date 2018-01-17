#include "motion_search_widget.h"
#include "private/motion_search_widget_p.h"

namespace nx {
namespace client {
namespace desktop {

MotionSearchWidget::MotionSearchWidget(QWidget* parent):
    base_type(parent),
    d(new Private(this))
{
}

MotionSearchWidget::~MotionSearchWidget()
{
}

QnVirtualCameraResourcePtr MotionSearchWidget::camera() const
{
    return d->camera();
}

void MotionSearchWidget::setCamera(const QnVirtualCameraResourcePtr& camera)
{
    d->setCamera(camera);
}

} // namespace desktop
} // namespace client
} // namespace nx
