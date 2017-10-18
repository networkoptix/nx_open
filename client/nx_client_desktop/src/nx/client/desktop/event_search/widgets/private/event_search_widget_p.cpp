#include "event_search_widget_p.h"

namespace nx {
namespace client {
namespace desktop {

EventSearchWidget::Private::Private(EventSearchWidget* q):
    QObject(),
    q(q)
{
}

EventSearchWidget::Private::~Private()
{
}

QnVirtualCameraResourcePtr EventSearchWidget::Private::camera() const
{
    return m_camera;
}

void EventSearchWidget::Private::setCamera(const QnVirtualCameraResourcePtr& camera)
{
    if (m_camera == camera)
        return;

    m_camera = camera;
}

} // namespace
} // namespace client
} // namespace nx
