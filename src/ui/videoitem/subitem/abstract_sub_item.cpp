#include "abstract_sub_item.h"
#include "..\abstract_scene_item.h"
#include <QPropertyAnimation>
#include <QGraphicsSceneMouseEvent>

#define OPACITY_TIME 500

CLAbstractSubItem::CLAbstractSubItem(CLAbstractSceneItem* parent, qreal opacity):
QGraphicsItem(static_cast<QGraphicsItem*>(parent)),
m_opacity(opacity),
m_animation(0)
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


void CLAbstractSubItem::hoverEnterEvent(QGraphicsSceneHoverEvent *event)
{
	stopAnimation();

	m_animation = new QPropertyAnimation(this, "opacity");
	m_animation->setDuration(OPACITY_TIME);
	m_animation->setStartValue(opacity());
	m_animation->setEndValue(1.0);
	m_animation->start();	
	connect(m_animation, SIGNAL(finished ()), this, SLOT(stopAnimation()));

}

void CLAbstractSubItem::hoverLeaveEvent(QGraphicsSceneHoverEvent *event)
{
	stopAnimation();

	m_animation = new QPropertyAnimation(this, "opacity");
	m_animation->setDuration(OPACITY_TIME);
	m_animation->setStartValue(opacity());
	m_animation->setEndValue(m_opacity);
	m_animation->start();	
	connect(m_animation, SIGNAL(finished ()), this, SLOT(stopAnimation()));

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
