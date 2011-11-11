#include "mouse_state.h"
#include <math.h>

void CLMouseState::mouseMoveEventHandler(QMouseEvent *event)
{
	while (m_mouseTrackQueue.size() > 10)
		m_mouseTrackQueue.dequeue();

	m_mouseTrackQueue.enqueue(QPair<QPoint, QTime> (event->pos(), QTime::currentTime()));
}

void CLMouseState::clearEvents()
{
	m_mouseTrackQueue.clear();
}

void CLMouseState::getMouseSpeed(qreal& speed, qreal& h_speed, qreal& v_speed) 
{
	h_speed = 0;
	v_speed = 0;
	speed = 0;

	if (m_mouseTrackQueue.isEmpty())
		return ;

	QTime firstTime = m_mouseTrackQueue.head().second;
	QPoint firstPoint = m_mouseTrackQueue.head().first;
	while (m_mouseTrackQueue.size() > 1) 
		m_mouseTrackQueue.dequeue();
	QTime lastTime = m_mouseTrackQueue.head().second;
	QPoint lastPoint = m_mouseTrackQueue.head().first;

	int dx = lastPoint.x() - firstPoint.x();
	int dy = lastPoint.y() - firstPoint.y();
	int idt = firstTime.msecsTo(lastTime);

	if (idt == 0)
		idt = 1;

	qreal dt = idt/ 1000.0;

	qreal dst = sqrt((qreal)dx*dx + (qreal)dy*dy);

	const qreal correction = 1.05;

	speed = correction*dst / dt;
	h_speed = correction*dx / dt;
	v_speed = correction*dy / dt;

	const qreal max_speed = 4000.0;
	if (speed>max_speed)
	{
		qreal factor = speed/max_speed;

		speed/=factor;
		h_speed/=factor;
		v_speed/=factor;
	}

}

QPoint CLMouseState::getLastEventPoint() const
{
	if (m_mouseTrackQueue.isEmpty())
		return QPoint(0,0);

	return m_mouseTrackQueue.last().first;

}
