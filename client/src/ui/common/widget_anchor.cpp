#include "widget_anchor.h"

#include <nx/utils/log/assert.h>


QnWidgetAnchor::QnWidgetAnchor(QWidget* widget) :
    base_type(widget),
    m_edges(Qt::LeftEdge | Qt::TopEdge | Qt::RightEdge | Qt::BottomEdge),
    m_margins(0, 0, 0, 0),
    m_widget(widget)
{
    NX_CRITICAL(widget);
    widget->installEventFilter(this);

    QWidget* parentWidget = widget->parentWidget();
    if (parentWidget)
        parentWidget->installEventFilter(this);
}

void QnWidgetAnchor::setEdges(Qt::Edges edges)
{
    if (m_edges == edges)
        return;

    m_edges = edges;
    updateGeometry();
}

Qt::Edges QnWidgetAnchor::edges() const
{
    return m_edges;
}

void QnWidgetAnchor::setMargins(const QMargins& margins)
{
    if (m_margins == margins)
        return;

    m_margins = margins;
    updateGeometry();
}

void QnWidgetAnchor::setMargins(int left, int top, int right, int bottom)
{
    setMargins(QMargins(left, top, right, bottom));
}

const QMargins& QnWidgetAnchor::margins() const
{
    return m_margins;
}

bool QnWidgetAnchor::eventFilter(QObject* object, QEvent* event)
{
    QWidget* parentWidget = m_widget->parentWidget();
    if (object == m_widget)
    {
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
        }
    }
    else
    {
        NX_ASSERT(object == parentWidget);

        if (event->type() == QEvent::Resize)
            updateGeometry();
    }

    return base_type::eventFilter(object, event);
}

void QnWidgetAnchor::updateGeometry()
{
    if (m_edges == 0)
        return;

    QWidget* parentWidget = m_widget->parentWidget();
    if (parentWidget)
    {
        if (parentWidget->layout())
        {
            qWarning() << "QnWidgetAnchor: widget is controlled by a layout";
            return;
        }

        QRect geometry = m_widget->geometry();
        QSize parentSize = parentWidget->size();

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

        QSize minimumSize = m_widget->minimumSize();
        geometry.setWidth(qMax(geometry.width(), minimumSize.width()));
        geometry.setHeight(qMax(geometry.height(), minimumSize.height()));

        m_widget->setGeometry(geometry);
    }
}

