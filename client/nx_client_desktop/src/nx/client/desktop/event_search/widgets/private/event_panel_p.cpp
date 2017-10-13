#include "event_panel_p.h"

#include <QtWidgets/QVBoxLayout>

#include <core/resource/camera_resource.h>
#include <ui/style/custom_style.h>

#include <nx/client/desktop/event_search/widgets/notification_list_widget.h>
#include <nx/client/desktop/event_search/widgets/event_search_widget.h>
#include <nx/client/desktop/event_search/models/event_list_model.h>
#include <nx/client/desktop/event_search/models/notification_list_model.h>

namespace nx {
namespace client {
namespace desktop {

EventPanel::Private::Private(EventPanel* q):
    QObject(),
    q(q),
    m_tabs(new QTabWidget(q)),
    m_systemTab(new NotificationListWidget(m_tabs)),
    m_cameraTab(new EventSearchWidget(m_tabs)),
    m_notificationsModel(new NotificationListModel(q)),
    m_eventSearchModel(new EventListModel(q))
{
    auto layout = new QVBoxLayout(q);
    layout->setContentsMargins(QMargins());
    layout->addWidget(m_tabs);
    m_tabs->addTab(m_systemTab, tr("System", "System events tab title"));

    setTabShape(m_tabs->tabBar(), style::TabShape::Compact);
    m_tabs->setProperty(style::Properties::kTabBarIndent, style::Metrics::kStandardPadding);

    q->setAutoFillBackground(false);
    q->setAttribute(Qt::WA_TranslucentBackground);
}

EventPanel::Private::~Private()
{
}

QnVirtualCameraResourcePtr EventPanel::Private::camera() const
{
    return m_camera;
}

void EventPanel::Private::setCamera(const QnVirtualCameraResourcePtr& camera)
{
    if (m_camera == camera)
        return;

    m_camera = camera;

    if (m_camera)
    {
        const auto index = m_tabs->indexOf(m_cameraTab);
        if (index < 0)
            m_tabs->addTab(m_cameraTab, m_camera->getName());
    }
    else
    {
        const auto index = m_tabs->indexOf(m_cameraTab);
        if (index >= 0)
            m_tabs->removeTab(index);
    }
}

} // namespace
} // namespace client
} // namespace nx
