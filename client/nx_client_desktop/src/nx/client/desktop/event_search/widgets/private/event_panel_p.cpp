#include "event_panel_p.h"

#include <QtGui/QPainter>
#include <QtWidgets/QVBoxLayout>

#include <core/resource/camera_resource.h>
#include <ui/style/custom_style.h>

#include <nx/client/desktop/event_search/widgets/notification_list_widget.h>
#include <nx/client/desktop/event_search/widgets/event_search_widget.h>

namespace nx {
namespace client {
namespace desktop {

EventPanel::Private::Private(EventPanel* q):
    QObject(),
    q(q),
    m_tabs(new QTabWidget(q)),
    m_systemTab(new NotificationListWidget(m_tabs)),
    m_cameraTab(new EventSearchWidget(m_tabs))
{
    auto layout = new QVBoxLayout(q);
    layout->setContentsMargins(QMargins());
    layout->addWidget(m_tabs);
    m_tabs->addTab(m_systemTab, tr("System", "System events tab title"));

    setTabShape(m_tabs->tabBar(), style::TabShape::Compact);
    m_tabs->setProperty(style::Properties::kTabBarIndent, style::Metrics::kStandardPadding);

    q->setAutoFillBackground(false);
    q->setAttribute(Qt::WA_TranslucentBackground);

    m_cameraTab->setHidden(true);
}

EventPanel::Private::~Private()
{
}

QnVirtualCameraResourcePtr EventPanel::Private::camera() const
{
    return m_cameraTab->camera();
}

void EventPanel::Private::setCamera(const QnVirtualCameraResourcePtr& camera)
{
    if (this->camera() == camera)
        return;

    m_cameraTab->setCamera(camera);

    const auto index = m_tabs->indexOf(m_cameraTab);
    if (camera)
    {
        if (index < 0)
            m_tabs->addTab(m_cameraTab, camera->getName());
        else
            m_tabs->setTabText(index, camera->getName());
    }
    else
    {
        if (index >= 0)
            m_tabs->removeTab(index);
    }
}

void EventPanel::Private::paintBackground()
{
    if (m_tabs->currentWidget() != m_cameraTab)
        return;

    QPainter painter(q);
    painter.fillRect(q->rect(), q->palette().background());
}

} // namespace
} // namespace client
} // namespace nx
