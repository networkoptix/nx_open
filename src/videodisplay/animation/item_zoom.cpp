#include "item_zoom.h"
#include <QGraphicsItem>
#include "../../base/log.h"
#include <math.h>


CLItemTransform::CLItemTransform(QGraphicsItem* item):
m_item(item),
m_zoom(1.0),
m_diff(0)
{
	//m_timeline.setCurveShape(QTimeLine::CurveShape::LinearCurve);
	m_timeline.setCurve(CLAnimationTimeLine::CLAnimationCurve::SLOW_END_POW_30);
	const int dur = 300;
	m_timeline.setDuration(dur);
	setDefaultDuration(dur);
}

CLItemTransform::~CLItemTransform()
{
	stop();
}

qreal CLItemTransform::zoomToscale(qreal zoom) const
{
	return zoom;
}

qreal CLItemTransform::scaleTozoom(qreal scale) const
{
	return scale;
}

void CLItemTransform::zoom(qreal target_zoom, bool instatntly)
{
	if (instatntly)
	{
		m_timeline.stop();

		m_zoom = target_zoom;
		zoom_helper();
	}
	else
	{
		m_start_point = m_zoom;
		m_diff = target_zoom - m_zoom;

		if (!isRuning())
		{
			m_timeline.start();
			m_timeline.start();
		}

		m_timeline.setCurrentTime(0);
	}


}


void CLItemTransform::onNewFrame(int frame)
{
	
	qreal pos = qreal(frame)/m_timeline.endFrame();
	m_zoom = m_start_point + pos*m_diff;
	zoom_helper();

}

void CLItemTransform::zoom_helper()
{
	QPointF center = m_item->boundingRect().center();

	QTransform trans;
	trans.translate(center.x(), center.y());

	qreal scl = zoomToscale(m_zoom);

	trans.scale(scl, scl);
	//trans.rotate((m_zoom-1)*2800); // specef
	//trans.rotate(m_zoom/6.95, Qt::YAxis); // specef
	//trans.rotate(m_zoom/6.95, Qt::XAxis); // specef
	trans.translate(-center.x(), -center.y());
	m_item->setTransform(trans);

}