#include <math.h>
#include "abstract_unmoved_opacity_item.h"



CLAbstractUnMovedOpacityItem::CLAbstractUnMovedOpacityItem(QString name, QGraphicsItem* parent):
CLAbstractUnmovedItem(name, parent),
m_animation(0)
{
}

CLAbstractUnMovedOpacityItem::~CLAbstractUnMovedOpacityItem()
{
	stopAnimation();
}

void CLAbstractUnMovedOpacityItem::changeOpacity(qreal new_opacity, int duration_ms)
{
    stopAnimation();

    if (duration_ms==0)
    {
        setOpacity(new_opacity);
        return;
    }

    m_animation = new QPropertyAnimation(this, "opacity");
    m_animation->setDuration(duration_ms);
    m_animation->setStartValue(opacity());
    m_animation->setEndValue(new_opacity);
    m_animation->start();	
    connect(m_animation, SIGNAL(finished ()), this, SLOT(stopAnimation()));

}


void CLAbstractUnMovedOpacityItem::stopAnimation()
{
	if (m_animation)
	{
		m_animation->stop();
		delete m_animation;
		m_animation = 0;
	}
}
