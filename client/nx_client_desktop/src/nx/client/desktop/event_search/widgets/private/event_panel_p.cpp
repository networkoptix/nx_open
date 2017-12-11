#include "event_panel_p.h"

#include <QtGui/QPainter>
#include <QtWidgets/QStackedWidget>
#include <QtWidgets/QTabWidget>
#include <QtWidgets/QVBoxLayout>

#include <core/resource/camera_resource.h>
#include <ui/style/custom_style.h>
#include <ui/graphics/items/resource/media_resource_widget.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_display.h>

#include <nx/client/desktop/event_search/widgets/event_search_widget.h>
#include <nx/client/desktop/event_search/widgets/motion_search_widget.h>
#include <nx/client/desktop/event_search/widgets/notification_list_widget.h>

namespace nx {
namespace client {
namespace desktop {

EventPanel::Private::Private(EventPanel* q):
    QObject(),
    q(q),
    m_tabs(new QTabWidget(q)),
    m_systemTab(new NotificationListWidget(m_tabs)),
    m_cameraTab(new QStackedWidget(m_tabs)),
    m_eventsWidget(new EventSearchWidget(m_cameraTab)),
    m_motionWidget(new MotionSearchWidget(m_cameraTab))
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
    m_cameraTab->addWidget(m_eventsWidget);
    m_cameraTab->addWidget(m_motionWidget);

    connect(q->context()->display(), &QnWorkbenchDisplay::widgetChanged,
        this, &Private::currentWorkbenchWidgetChanged, Qt::QueuedConnection);

    connect(m_eventsWidget, &EventSearchWidget::analyticsSearchByAreaEnabledChanged, this,
        [this](bool enabled)
        {
            if (m_currentMediaWidget)
                m_currentMediaWidget->setAnalyticsSearchModeEnabled(enabled);
        });
}

EventPanel::Private::~Private()
{
}

QnVirtualCameraResourcePtr EventPanel::Private::camera() const
{
    return m_eventsWidget->camera();
}

void EventPanel::Private::setCamera(const QnVirtualCameraResourcePtr& camera)
{
    if (this->camera() == camera)
        return;

    m_eventsWidget->setCamera(camera);
    m_motionWidget->setCamera(camera);

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

void EventPanel::Private::currentWorkbenchWidgetChanged(Qn::ItemRole role)
{
    if (role != Qn::CentralRole)
        return;

    m_mediaWidgetConnections.reset();

    m_eventsWidget->setAnalyticsSearchByAreaEnabled(false);

    m_currentMediaWidget = qobject_cast<QnMediaResourceWidget*>(
        this->q->context()->display()->widget(Qn::CentralRole));

    const auto camera = m_currentMediaWidget
        ? m_currentMediaWidget->resource().dynamicCast<QnVirtualCameraResource>()
        : QnVirtualCameraResourcePtr();

    setCamera(camera);

    if (!camera)
        return;

    m_mediaWidgetConnections.reset(new QnDisconnectHelper());

    *m_mediaWidgetConnections << connect(
        m_currentMediaWidget.data(), &QnMediaResourceWidget::motionSearchModeEnabled, this,
        [this](bool enabled)
        {
            if (enabled)
                m_cameraTab->setCurrentWidget(m_motionWidget);
            else
                m_cameraTab->setCurrentWidget(m_eventsWidget);
        });

    *m_mediaWidgetConnections << connect(
        m_currentMediaWidget.data(), &QnMediaResourceWidget::analyticsSearchAreaSelected, this,
        [this](const QRectF& relativeRect)
        {
            m_eventsWidget->setAnalyticsSearchRect(relativeRect);
        });
}

} // namespace
} // namespace client
} // namespace nx
