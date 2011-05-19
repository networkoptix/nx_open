#include "item_trans.h"
#include "../../base/log.h"
#include <math.h>

qreal round_angle(qreal angle, qreal min_diff)
{
	qreal result = angle;

	int angle_int_90 = (int)angle/90.0;

	qreal angle_mod_90 = abs(angle - angle_int_90*90);

	if (angle_mod_90 < min_diff)
		result = angle_int_90*90;	

	 if( abs(angle_mod_90 - 90) < min_diff)
		 result = angle_int_90*90 + (angle>0 ? 90 : -90);	

	return result;
}

//=========================================================
CLItemTransform::CLItemTransform(QGraphicsItem* item):
m_item(item),
m_zoom(1.0),
m_Zrotation(0.0)
{
	
}

CLItemTransform::~CLItemTransform()
{
	stopAnimation();
}


qreal CLItemTransform::zoomToscale(qreal zoom) const
{
	return zoom;
}

qreal CLItemTransform::scaleTozoom(qreal scale) const
{
	return scale;
}

void CLItemTransform::move_abs(QPointF pos, int duration, int delay )
{
	//m_moving.start_point = m_item->scenePos()
}

qreal CLItemTransform::getRotation() const
{
	return m_Zrotation;
}

qreal CLItemTransform::getZoom() const
{
	return m_zoom;
}

void CLItemTransform::setRotation(qreal r)
{
    m_Zrotation = r;
    transform_helper();
}

void CLItemTransform::setZoom(qreal z)
{
    m_zoom = z;
    transform_helper();
}


void CLItemTransform::zoom_abs(qreal target_zoom, int duration, int delay)
{

    stopAnimation();

	if (duration==0)
	{
        setZoom(target_zoom);
	}
	else
	{
        m_animation = new QPropertyAnimation(this, "zoom");
        QPropertyAnimation* panimation = static_cast<QPropertyAnimation*>(m_animation);
        panimation->setStartValue(this->getZoom());
        panimation->setEndValue(target_zoom);
        panimation->setDuration(duration);
        panimation->setEasingCurve(easingCurve(SLOW_END_POW_30));
		start_helper(duration, delay);
	}

}

void CLItemTransform::z_rotate_abs(QPointF center, qreal angle, int duration, int delay)
{

    stopAnimation();

    m_rotatePoint = center;

	if (duration==0)
	{
		
		m_Zrotation = angle;
		m_Zrotation = m_Zrotation - ((int)m_Zrotation/360)*360;
		transform_helper();
	}
	else
	{
		
		if (getRotation() > 180 && angle==0.)
			angle = 360.;

        m_animation = new QPropertyAnimation(this, "rotation");
        QPropertyAnimation* panimation = static_cast<QPropertyAnimation*>(m_animation);
        panimation->setStartValue(getRotation());
        panimation->setEndValue(angle);
        panimation->setDuration(duration);
        panimation->setEasingCurve(easingCurve(SLOW_END_POW_30));
		start_helper(duration, delay);

	}
}

void CLItemTransform::z_rotate_delta(QPointF center, qreal angle, int duration, int delay)
{

	//=============
	qreal final = m_Zrotation + angle;
    z_rotate_abs(center, final, duration, delay);

}

void CLItemTransform::transform_helper()
{
	QPointF center = m_item->boundingRect().center();

	qreal scl = zoomToscale(m_zoom);

	QTransform trans;
	/*
	trans.translate(center.x(), center.y());
	trans.scale(scl, scl);
	trans.rotate(m_Zrotation.curent); 
	trans.translate(-center.x(), -center.y());
	/**/

	qreal transform_ange = round_angle(m_Zrotation, 1);

	trans.translate(m_rotatePoint.x(), m_rotatePoint.y());
	trans.rotate(transform_ange); 
	trans.translate(-m_rotatePoint.x(), -m_rotatePoint.y());

	trans.translate(center.x(), center.y());
	trans.scale(scl, scl);
	trans.translate(-center.x(), -center.y());

	/**/

	m_item->setTransform(trans);

	//QPointF pf = m_item->mapToScene(m_item->boundingRect().topLeft());
	//cl_log.log("left = ",  (int)pf.x(), cl_logDEBUG1 );

}
