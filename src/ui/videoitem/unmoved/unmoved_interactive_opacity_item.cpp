#include "unmoved_interactive_opacity_item.h"

extern int global_opacity_change_period;

CLUnMovedInteractiveOpacityItem::CLUnMovedInteractiveOpacityItem(QString name, QGraphicsItem* parent, qreal normal_opacity, qreal active_opacity):
    CLAbstractUnMovedOpacityItem(name, parent),
    m_normal_opacity(normal_opacity),
    m_active_opacity(active_opacity)
{
    setOpacity(m_normal_opacity);
}

CLUnMovedInteractiveOpacityItem::~CLUnMovedInteractiveOpacityItem()
{
    stopAnimation();
}

void CLUnMovedInteractiveOpacityItem::hoverEnterEvent(QGraphicsSceneHoverEvent *event)
{
    CLAbstractUnMovedOpacityItem::hoverEnterEvent(event);

    changeOpacity(m_active_opacity, global_opacity_change_period);
}

void CLUnMovedInteractiveOpacityItem::hoverLeaveEvent(QGraphicsSceneHoverEvent *event)
{
    CLAbstractUnMovedOpacityItem::hoverLeaveEvent(event);

    changeOpacity(m_normal_opacity, global_opacity_change_period);
}

void CLUnMovedInteractiveOpacityItem::hide(int duration)
{
    changeOpacity(0, duration);
}

void CLUnMovedInteractiveOpacityItem::show(int duration)
{
    changeOpacity(m_normal_opacity, duration);
}
