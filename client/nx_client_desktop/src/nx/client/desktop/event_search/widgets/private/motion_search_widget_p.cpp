#include "motion_search_widget_p.h"

#include <nx/client/desktop/event_search/models/motion_search_list_model.h>

namespace nx {
namespace client {
namespace desktop {

MotionSearchWidget::Private::Private(MotionSearchWidget* q):
    base_type(q),
    q(q),
    m_model(new MotionSearchListModel(q))
{
    // TODO: #vkutin This is temporary. The header will be used soon.
    m_headerWidget->setHidden(true);
    setModel(m_model);
}

MotionSearchWidget::Private::~Private()
{
}

QnVirtualCameraResourcePtr MotionSearchWidget::Private::camera() const
{
    return m_model->camera();
}

void MotionSearchWidget::Private::setCamera(const QnVirtualCameraResourcePtr& camera)
{
    m_model->setCamera(camera);
}

} // namespace desktop
} // namespace client
} // namespace nx
