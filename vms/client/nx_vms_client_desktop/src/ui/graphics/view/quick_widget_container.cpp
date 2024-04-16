// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "quick_widget_container.h"

#include <ui/graphics/view/graphics_view.h>

namespace nx::vms::client::desktop {

void QuickWidgetContainer::setQuickWidget(QQuickWidget* quickWidget)
{
    m_quickWidget = quickWidget;
    m_quickWidget->setParent(this);
    m_quickWidget->setGeometry(0, 0, width(), height());

    setAttribute(Qt::WA_OpaquePaintEvent);
    setAttribute(Qt::WA_NoSystemBackground);
    setAttribute(Qt::WA_DontShowOnScreen);
}

QQuickWidget* QuickWidgetContainer::quickWidget() const
{
    return m_quickWidget;
}

void QuickWidgetContainer::setView(QWidget* view)
{
    m_view = view;
}

QWidget* QuickWidgetContainer::view() const
{
    return m_view;
}

bool QuickWidgetContainer::event(QEvent* event)
{
    if (event->type() == QEvent::Paint)
    {
        if (auto view = qobject_cast<QnGraphicsView*>(m_view))
        {
            view->paintRhi();
            return true;
        }
    }
    return base_type::event(event);
}

QPaintEngine* QuickWidgetContainer::paintEngine() const
{
    return nullptr;
}

void QuickWidgetContainer::resizeEvent(QResizeEvent* event)
{
    if (m_quickWidget)
        m_quickWidget->setGeometry(QRect(QPoint(0, 0), event->size()));
}

void QuickWidgetContainer::moveEvent(QMoveEvent*)
{
    if (m_quickWidget)
        m_quickWidget->setGeometry(QRect(QPoint(0, 0), size()));
}

void QuickWidgetContainer::showEvent(QShowEvent*)
{
    if (m_quickWidget)
        m_quickWidget->setGeometry(QRect(QPoint(0, 0), size()));
}

} // namespace nx::vms::client::desktop
