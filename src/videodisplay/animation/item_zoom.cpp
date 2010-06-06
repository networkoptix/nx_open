#include "item_zoom.h"
#include <QGraphicsItem>
#include "../../base/log.h"


CLItemZoom::CLItemZoom(QGraphicsItem* item):
m_item(item),
m_zoom(0),
m_diff(0)
{
	//m_timeline.setCurveShape(QTimeLine::CurveShape::LinearCurve);

	const int dur = 300;

	m_timeline.setDuration(dur);
	setDefaultDuration(dur);
	m_timeline.setFrameRange(0, 6000);
}

CLItemZoom::~CLItemZoom()
{
	stop();
}

void CLItemZoom::zoom(int target_zoom)
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


void CLItemZoom::onNewFrame(int frame)
{
	
	qreal pos = qreal(frame)/m_timeline.endFrame();

	
	m_zoom = m_start_point + pos*m_diff;

	QPointF center = m_item->boundingRect().center();

	QTransform trans;
	trans.translate(center.x(), center.y());
	trans.scale(1 + m_zoom/ 3300.0, 1 + m_zoom/ 3300.0);
	trans.translate(-center.x(), -center.y());
	m_item->setTransform(trans);

}

