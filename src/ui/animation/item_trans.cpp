#include "item_trans.h"
#include <QGraphicsItem>
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
m_Zrotation(0.0),
m_movment(QPointF(0,0)),
m_zooming(false),
m_rotating(false),
m_moving(false)
{
	m_timeline.setCurve(CLAnimationTimeLine::CLAnimationCurve::SLOW_END_POW_30);
}

CLItemTransform::~CLItemTransform()
{
	stop();
}

void CLItemTransform::stop()
{
	CLAnimation::stop();
	m_rotating = false;
	m_zooming = false;
	m_moving = false;
}

void CLItemTransform::onFinished()
{
	CLAnimation::onFinished();
	m_zooming = false;
	m_rotating = false;
	m_moving = false;
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

qreal CLItemTransform::current_zrotation() const
{
	return m_Zrotation.curent;
}

qreal CLItemTransform::current_zoom() const
{
	return m_zoom.curent;
}

void CLItemTransform::zoom_abs(qreal target_zoom, int duration, int delay)
{

	if (m_rotating)
	{
		duration = 0; // do it instantly
		m_Zrotation.curent = m_Zrotation.start_point + m_Zrotation.diff;
	}

	if (duration==0)
	{
		stop();

		m_zoom.curent = target_zoom;
		transform_helper();
	}
	else
	{
		m_zoom.start_point = m_zoom.curent;
		m_zoom.diff = target_zoom - m_zoom.curent;

		start_helper(duration, delay);

		m_zooming = true;


	}


}

void CLItemTransform::z_rotate_abs(QPointF center, qreal angle, int duration, int delay)
{
	if (m_zooming)
		duration = 0; // do it instantly
	
	if (duration==0)
	{
		m_rotatePoint = center;
		m_Zrotation.curent = angle;
		m_Zrotation.curent = m_Zrotation.curent - ((int)m_Zrotation.curent/360)*360;
		transform_helper();
	}
	else
	{
		m_rotatePoint = center;
		m_Zrotation.start_point = m_Zrotation.curent;

		if (m_Zrotation.start_point>180 && angle==0.)
			angle = 360.;

		m_Zrotation.diff = angle - m_Zrotation.start_point;

		start_helper(duration, delay);
		m_rotating = true;

		

	}
}

void CLItemTransform::z_rotate_delta(QPointF center, qreal angle, int duration, int delay)
{
	if (m_zooming)
		duration = 0; // do it instantly
	
	//=============

	if (duration!=0) // if animation
	{
		qreal final = m_Zrotation.curent + angle;
		angle = final - m_Zrotation.curent;
	}

	//=============

	if (duration==0)
	{
		m_rotatePoint = center;
		m_Zrotation.curent += angle;
		m_Zrotation.curent = m_Zrotation.curent - ((int)m_Zrotation.curent/360)*360;
		transform_helper();
	}
	else
	{
		m_rotatePoint = center;
		m_Zrotation.start_point = m_Zrotation.curent;
		m_Zrotation.diff = angle;

		start_helper(duration, delay);

		m_rotating = true;

		

	}

	
}

void CLItemTransform::valueChanged( qreal pos )
{
	if (m_zooming)
		m_zoom.curent = m_zoom.start_point + pos*m_zoom.diff;

	if (m_rotating)
		m_Zrotation.curent = m_Zrotation.start_point + pos*m_Zrotation.diff;

	transform_helper();

}

void CLItemTransform::transform_helper()
{
	QPointF center = m_item->boundingRect().center();

	qreal scl = zoomToscale(m_zoom.curent);

	QTransform trans;
	/*
	trans.translate(center.x(), center.y());
	trans.scale(scl, scl);
	trans.rotate(m_Zrotation.curent); 
	trans.translate(-center.x(), -center.y());
	/**/

	qreal transform_ange = round_angle(m_Zrotation.curent, 1);
	
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