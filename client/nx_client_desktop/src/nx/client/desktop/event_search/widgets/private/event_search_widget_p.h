#pragma once

#include "../event_search_widget.h"

#include <QtWidgets/QTabWidget>

namespace nx {
namespace client {
namespace desktop {

class EventSearchWidget::Private: public QObject
{
    Q_OBJECT

public:
    Private(EventSearchWidget* q);
    virtual ~Private() override;

    QnVirtualCameraResourcePtr camera() const;
    void setCamera(const QnVirtualCameraResourcePtr& camera);

private:
    EventSearchWidget* q = nullptr;
    QnVirtualCameraResourcePtr m_camera;
};

} // namespace
} // namespace client
} // namespace nx
