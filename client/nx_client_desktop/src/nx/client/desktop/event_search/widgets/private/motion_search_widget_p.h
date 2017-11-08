#pragma once

#include "../motion_search_widget.h"
#include "search_widget_base_p.h"

namespace nx {
namespace client {
namespace desktop {

class EventRibbon;
class MotionSearchListModel;

class MotionSearchWidget::Private: public detail::EventSearchWidgetPrivateBase
{
    Q_OBJECT
    using base_type = detail::EventSearchWidgetPrivateBase;

public:
    Private(MotionSearchWidget* q);
    virtual ~Private() override;

    QnVirtualCameraResourcePtr camera() const;
    void setCamera(const QnVirtualCameraResourcePtr& camera);

private:
    MotionSearchWidget* q = nullptr;
    MotionSearchListModel* const m_model = nullptr;
};

} // namespace
} // namespace client
} // namespace nx
