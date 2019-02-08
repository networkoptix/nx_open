#include "widget_anchor.h"

#include <QtCore/QEvent>
#include <QtWidgets/QWidget>

#include <nx/utils/log/assert.h>

namespace nx::vms::client::desktop {

WidgetAnchor::WidgetAnchor(QWidget* widget):
    base_type(widget),
    m_widget(widget)
{
    NX_ASSERT(widget);
    if (!widget)
        return;

    widget->installEventFilter(this);

    const auto parentWidget = widget->parentWidget();
    if (parentWidget)
        parentWidget->installEventFilter(this);
}

void WidgetAnchor::setEdges(Qt::Edges edges)
{
    if (m_edges == edges)
        return;

    m_edges = edges;
    updateGeometry();
}

Qt::Edges WidgetAnchor::edges() const
{
    return m_edges;
}

void WidgetAnchor::setMargins(const QMargins& margins)
{
    if (m_margins == margins)
        return;

    m_margins = margins;
    updateGeometry();
}

void WidgetAnchor::setMargins(int left, int top, int right, int bottom)
{
    setMargins(QMargins(left, top, right, bottom));
}

const QMargins& WidgetAnchor::margins() const
{
    return m_margins;
}

bool WidgetAnchor::eventFilter(QObject* object, QEvent* event)
{
    if (object == m_widget)
    {
        const auto parentWidget = m_widget->parentWidget();
        switch (event->type())
        {
            case QEvent::Resize:
            case QEvent::Move:
                updateGeometry();
                break;

            case QEvent::ParentAboutToChange:
                if (parentWidget)
                    parentWidget->removeEventFilter(this);
                break;

            case QEvent::ParentChange:
            {
                if (parentWidget)
                {
                    parentWidget->installEventFilter(this);
                    updateGeometry();
                }
                break;
            }

            default:
                break;
        }
    }
    else
    {
        if (event->type() == QEvent::Resize && !m_widget.isNull()
            && object == m_widget->parentWidget())
        {
            updateGeometry();
        }
    }

    return base_type::eventFilter(object, event);
}

void WidgetAnchor::updateGeometry()
{
    if (!m_edges || !m_widget)
        return;

    const auto parentWidget = m_widget->parentWidget();
    if (!parentWidget)
        return;

    QRect geometry = m_widget->geometry();
    const auto parentSize = parentWidget->size();

    if (m_edges.testFlag(Qt::LeftEdge) && m_edges.testFlag(Qt::RightEdge))
    {
        geometry.setLeft(m_margins.left());
        geometry.setRight(parentSize.width() - m_margins.right() - 1);
    }
    else
    {
        if (m_edges.testFlag(Qt::LeftEdge))
            geometry.moveLeft(m_margins.left());
        else if (m_edges.testFlag(Qt::RightEdge))
            geometry.moveRight(parentSize.width() - m_margins.right() - 1);
    }

    if (m_edges.testFlag(Qt::TopEdge) && m_edges.testFlag(Qt::BottomEdge))
    {
        geometry.setTop(m_margins.top());
        geometry.setBottom(parentSize.height() - m_margins.bottom() - 1);
    }
    else
    {
        if (m_edges.testFlag(Qt::TopEdge))
            geometry.moveTop(m_margins.top());
        else if (m_edges.testFlag(Qt::BottomEdge))
            geometry.moveBottom(parentSize.height() - m_margins.bottom() - 1);
    }

    const auto minimumSize = m_widget->minimumSize();
    geometry.setWidth(qMax(geometry.width(), minimumSize.width()));
    geometry.setHeight(qMax(geometry.height(), minimumSize.height()));

    m_widget->setGeometry(geometry);
}

WidgetAnchor* anchorWidgetToParent(QWidget* widget, Qt::Edges edges, const QMargins& margins)
{
    NX_ASSERT(widget);
    if (!widget)
        return nullptr;

    auto anchor = new WidgetAnchor(widget);
    anchor->setEdges(edges);
    anchor->setMargins(margins);
    return anchor;
}

WidgetAnchor* anchorWidgetToParent(QWidget* widget, const QMargins& margins)
{
    return anchorWidgetToParent(widget, WidgetAnchor::kAllEdges, margins);
}

} // namespace nx::vms::client::desktop
