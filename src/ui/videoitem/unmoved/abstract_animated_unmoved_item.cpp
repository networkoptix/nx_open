#include "abstract_animated_unmoved_item.h"
#include <math.h>

#define OPACITY_TIME 500



CLUnMovedOpacityItem::CLUnMovedOpacityItem(QString name, QGraphicsItem* parent, qreal normal_opacity, qreal active_opacity):
CLAbstractUnmovedItem(name, parent),
m_normal_opacity(normal_opacity),
m_active_opacity(active_opacity),
m_animation(0)
{
	setAcceptsHoverEvents(true);
	setOpacity(m_normal_opacity);
}

CLUnMovedOpacityItem::~CLUnMovedOpacityItem()
{
	stopAnimation();
}

void CLUnMovedOpacityItem::hoverEnterEvent(QGraphicsSceneHoverEvent *event)
{
	if (needAnimation())
	{
		stopAnimation();
		m_animation = new QPropertyAnimation(this, "opacity");
		m_animation->setDuration(OPACITY_TIME);
		m_animation->setStartValue(opacity());
		m_animation->setEndValue(m_active_opacity);
		m_animation->start();	
		connect(m_animation, SIGNAL(finished ()), this, SLOT(stopAnimation()));
	}

}

void CLUnMovedOpacityItem::hoverLeaveEvent(QGraphicsSceneHoverEvent *event)
{
	if (needAnimation())
	{
		stopAnimation();
		m_animation = new QPropertyAnimation(this, "opacity");
		m_animation->setDuration(OPACITY_TIME);
		m_animation->setStartValue(opacity());
		m_animation->setEndValue(m_normal_opacity);
		m_animation->start();	
		connect(m_animation, SIGNAL(finished ()), this, SLOT(stopAnimation()));
	}

}


void CLUnMovedOpacityItem::stopAnimation()
{
	if (m_animation)
	{
		m_animation->stop();
		delete m_animation;
		m_animation = 0;
	}
}

bool CLUnMovedOpacityItem::needAnimation() const
{
	qreal diff = fabs(m_normal_opacity - m_active_opacity);

	if (diff<1e-4)
		return false;

	return true;

}