#include "abstract_sub_item.h"
#include "..\abstract_scene_item.h"
#include <QPropertyAnimation>
#include <QGraphicsSceneMouseEvent>
#include <math.h>

#define OPACITY_TIME 500

CLAbstractSubItem::CLAbstractSubItem(CLAbstractSceneItem* parent, qreal opacity, qreal max_opacity):
QGraphicsItem(static_cast<QGraphicsItem*>(parent)),
m_opacity(opacity),
m_maxopacity(max_opacity),
m_animation(0),
mParent(parent)
{
	setAcceptsHoverEvents(true);
	//setZValue(parent->zvalue() + 1);

	connect(this, SIGNAL(onPressed(CLAbstractSubItem*)), static_cast<QObject*>(parent), SLOT(onSubItemPressed(CLAbstractSubItem*)) );
	//onSubItemPressed
	setOpacity(m_opacity);
}

CLAbstractSubItem::~CLAbstractSubItem()
{
	stopAnimation();
}


CLAbstractSubItem::ItemType CLAbstractSubItem::getType() const
{
	return mType;
}

void CLAbstractSubItem::hoverEnterEvent(QGraphicsSceneHoverEvent *event)
{
	if (needAnimation())
	{
		stopAnimation();
		m_animation = new QPropertyAnimation(this, "opacity");
		m_animation->setDuration(OPACITY_TIME);
		m_animation->setStartValue(opacity());
		m_animation->setEndValue(m_maxopacity);
		m_animation->start();	
		connect(m_animation, SIGNAL(finished ()), this, SLOT(stopAnimation()));
	}

}

void CLAbstractSubItem::hoverLeaveEvent(QGraphicsSceneHoverEvent *event)
{
	if (needAnimation())
	{
		stopAnimation();
		m_animation = new QPropertyAnimation(this, "opacity");
		m_animation->setDuration(OPACITY_TIME);
		m_animation->setStartValue(opacity());
		m_animation->setEndValue(m_opacity);
		m_animation->start();	
		connect(m_animation, SIGNAL(finished ()), this, SLOT(stopAnimation()));
	}

}

void CLAbstractSubItem::mouseReleaseEvent( QGraphicsSceneMouseEvent * event )
{
	emit onPressed(this);
}

void CLAbstractSubItem::mousePressEvent( QGraphicsSceneMouseEvent * event )
{
	event->accept();
}

void CLAbstractSubItem::stopAnimation()
{
	if (m_animation)
	{
		m_animation->stop();
		delete m_animation;
		m_animation = 0;
	}
}

bool CLAbstractSubItem::needAnimation() const
{

	qreal diff = fabs(m_opacity - m_maxopacity);

	if (diff<1e-4)
		return false;

	return true;
}