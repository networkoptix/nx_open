#include "quick_item_mouse_tracker.h"

QnQuickItemMouseTracker::QnQuickItemMouseTracker(QQuickItem* parent)
    : QQuickItem(parent)
    , m_item(nullptr)
{
}

QnQuickItemMouseTracker::~QnQuickItemMouseTracker()
{
    if (m_item)
        m_item->removeEventFilter(this);
}

bool QnQuickItemMouseTracker::eventFilter(QObject* object, QEvent* event)
{
    if (object != m_item)
        return false;

    switch (event->type())
    {
    case QEvent::MouseMove:
    case QEvent::MouseButtonPress:
    case QEvent::MouseButtonRelease:
        {
            QMouseEvent* me = static_cast<QMouseEvent*>(event);
            setMousePosition(me->pos());
        }
        break;
    default:
        break;
    }
    return false;
}

qreal QnQuickItemMouseTracker::mouseX() const
{
    return m_mousePosition.x();
}

qreal QnQuickItemMouseTracker::mouseY() const
{
    return m_mousePosition.y();
}

QQuickItem* QnQuickItemMouseTracker::item() const
{
    return m_item;
}

void QnQuickItemMouseTracker::setItem(QQuickItem* item)
{
    if (m_item == item)
        return;

    if (m_item)
        m_item->removeEventFilter(this);

    m_item = item;
    if (m_item)
        m_item->installEventFilter(this);

    emit itemChanged();
}

void QnQuickItemMouseTracker::setMousePosition(const QPointF &pos)
{
    if (m_mousePosition == pos)
        return;

    m_mousePosition = pos;
    emit mouseXChanged();
    emit mouseYChanged();
    emit mousePositionChanged();
}
