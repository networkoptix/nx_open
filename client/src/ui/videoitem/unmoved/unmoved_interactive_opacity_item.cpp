#include "unmoved_interactive_opacity_item.h"
#include "ui/skin/globals.h"


CLUnMovedInteractiveOpacityItem::CLUnMovedInteractiveOpacityItem(QString name, QGraphicsItem* parent, qreal normal_opacity, qreal active_opacity):
    CLAbstractUnmovedItem(name, parent),
    m_normal_opacity(normal_opacity),
    m_active_opacity(active_opacity)
{
    setAcceptHoverEvents(true);

    setOpacity(m_normal_opacity);
}

CLUnMovedInteractiveOpacityItem::~CLUnMovedInteractiveOpacityItem()
{
}

void CLUnMovedInteractiveOpacityItem::setVisible(bool visible, int duration)
{
    changeOpacity(visible ? m_normal_opacity : 0.0, duration);
}

void CLUnMovedInteractiveOpacityItem::hoverEnterEvent(QGraphicsSceneHoverEvent *event)
{
    CLAbstractUnmovedItem::hoverEnterEvent(event);

    changeOpacity(m_active_opacity, Globals::opacityChangePeriod());
}

void CLUnMovedInteractiveOpacityItem::hoverLeaveEvent(QGraphicsSceneHoverEvent *event)
{
    CLAbstractUnmovedItem::hoverLeaveEvent(event);

    changeOpacity(m_normal_opacity, Globals::opacityChangePeriod());
}
