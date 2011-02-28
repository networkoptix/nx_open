#include <math.h>
#include "unmoved_interactive_opacity_item.h"

#define OPACITY_TIME 500



CLUnMovedInteractiveOpacityItem::CLUnMovedInteractiveOpacityItem(QString name, QGraphicsItem* parent, qreal normal_opacity, qreal active_opacity):
CLAbstractUnMovedOpacityItem(name, parent),
m_normal_opacity(normal_opacity),
m_active_opacity(active_opacity)
{
	setAcceptsHoverEvents(true);
	setOpacity(m_normal_opacity);
}

CLUnMovedInteractiveOpacityItem::~CLUnMovedInteractiveOpacityItem()
{
	stopAnimation();
}

void CLUnMovedInteractiveOpacityItem::hoverEnterEvent(QGraphicsSceneHoverEvent *event)
{
	if (needAnimation())
        changeOpacity(m_active_opacity, OPACITY_TIME);
}

void CLUnMovedInteractiveOpacityItem::hoverLeaveEvent(QGraphicsSceneHoverEvent *event)
{
	if (needAnimation())
        changeOpacity(m_normal_opacity, OPACITY_TIME);
}



bool CLUnMovedInteractiveOpacityItem::needAnimation() const
{
	qreal diff = fabs(m_normal_opacity - m_active_opacity);

	if (diff<1e-4)
		return false;

	return true;

}