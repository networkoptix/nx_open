#include "event_search_widget.h"
#include "private/event_search_widget_p.h"

namespace nx {
namespace client {
namespace desktop {

EventSearchWidget::EventSearchWidget(QWidget* parent):
    base_type(parent),
    d(new Private(this))
{
}

EventSearchWidget::~EventSearchWidget()
{
}

QnVirtualCameraResourcePtr EventSearchWidget::camera() const
{
    return d->camera();
}

void EventSearchWidget::setCamera(const QnVirtualCameraResourcePtr& camera)
{
    d->setCamera(camera);
}

void EventSearchWidget::setAnalyticsSearchRect(const QRectF& relativeRect)
{
    d->setAnalyticsSearchRect(relativeRect);
}

bool EventSearchWidget::analyticsSearchByAreaEnabled() const
{
    return d->analyticsSearchByAreaEnabled();
}

void EventSearchWidget::setAnalyticsSearchByAreaEnabled(bool value)
{
    d->setAnalyticsSearchByAreaEnabled(value);
}

} // namespace
} // namespace client
} // namespace nx
