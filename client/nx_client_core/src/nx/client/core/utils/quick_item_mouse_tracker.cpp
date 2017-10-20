#include "quick_item_mouse_tracker.h"

namespace nx {
namespace client {
namespace core {

QuickItemMouseTracker::QuickItemMouseTracker(QQuickItem* parent) :
    QQuickItem(parent)
{
}

QuickItemMouseTracker::~QuickItemMouseTracker()
{
    if (m_item)
        m_item->removeEventFilter(this);
}

bool QuickItemMouseTracker::eventFilter(QObject* object, QEvent* event)
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
            break;
        }

        case QEvent::HoverEnter:
            setContainsMouse(true);
            break;

        case QEvent::HoverLeave:
            setContainsMouse(false);
            break;

        default:
            break;
    }

    return false;
}

qreal QuickItemMouseTracker::mouseX() const
{
    return m_mousePosition.x();
}

qreal QuickItemMouseTracker::mouseY() const
{
    return m_mousePosition.y();
}

bool QuickItemMouseTracker::containsMouse() const
{
    return m_containsMouse;
}

bool QuickItemMouseTracker::hoverEventsEnabled() const
{
    return m_item && m_item->acceptHoverEvents();
}

void QuickItemMouseTracker::setHoverEventsEnabled(bool enabled)
{
    if (!m_item)
    {
        if (m_originalHoverEventsEnabled == enabled)
            return;

        m_originalHoverEventsEnabled = enabled;
        emit hoverEventsEnabledChanged();

        return;
    }

    if (m_item->acceptHoverEvents() == enabled)
        return;

    m_item->setAcceptHoverEvents(enabled);
    emit hoverEventsEnabledChanged();
}

QQuickItem* QuickItemMouseTracker::item() const
{
    return m_item;
}

void QuickItemMouseTracker::setItem(QQuickItem* item)
{
    if (m_item == item)
        return;

    if (m_item)
    {
        m_item->removeEventFilter(this);

        const bool hoverEventsEnabled = this->hoverEventsEnabled();
        m_item->setAcceptHoverEvents(m_originalHoverEventsEnabled);
        m_originalHoverEventsEnabled = hoverEventsEnabled;
    }

    m_item = item;
    if (m_item)
    {
        const bool hoverEventsEnabled = m_originalHoverEventsEnabled;
        m_originalHoverEventsEnabled = m_item->acceptHoverEvents();
        setHoverEventsEnabled(hoverEventsEnabled);

        m_item->installEventFilter(this);
    }

    emit hoverEventsEnabledChanged();
    emit itemChanged();
}

void QuickItemMouseTracker::setMousePosition(const QPointF &pos)
{
    if (m_mousePosition == pos)
        return;

    m_mousePosition = pos;
    emit mouseXChanged();
    emit mouseYChanged();
    emit mousePositionChanged();
}

void QuickItemMouseTracker::setContainsMouse(bool containsMouse)
{
    if (m_containsMouse == containsMouse)
        return;

    m_containsMouse = containsMouse;
    emit containsMouseChanged();
}

void QuickItemMouseTracker::registerQmlType()
{
    qmlRegisterType<QuickItemMouseTracker>("nx.client.core", 1, 0, "MouseTracker");
}

} // namespace core
} // namespace client
} // namespace nx
