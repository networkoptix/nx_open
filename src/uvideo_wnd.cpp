#include "uvideo_wnd.h"


#include <QPainter>

VideoWindow::VideoWindow(const CLDeviceVideoLayout* layout, int max_width, int max_height):
CLVideoWindow(layout, max_width, max_height),
m_selected(false),
m_z(0),
m_zoomTimeLine(CLAnimationTimeLine::CLAnimationCurve::SLOW_END)
{
	setAcceptsHoverEvents(true);

	m_zoomTimeLine.setDuration(400);
	m_zoomTimeLine.setFrameRange(0, 900);
	m_zoomTimeLine.setUpdateInterval(17); // 60 fps
	//m_zoomTimeLine.setCurveShape(QTimeLine::EaseOutCurve);

	connect(&m_zoomTimeLine, SIGNAL(frameChanged(int)), this, SLOT(setMouseMoveZoom(int)));
	connect(&m_zoomTimeLine, SIGNAL(finished()), this, SLOT(updateZvalue()));

}


void VideoWindow::setSelected(bool sel)
{
	m_selected = sel;
}


void VideoWindow::drawStuff(QPainter* painter)
{
	CLVideoWindow::drawStuff(painter);

	if (m_selected)
		drawSelection(painter);


}

void VideoWindow::drawSelection(QPainter* painter)
{

}


//===========================================

void VideoWindow::hoverEnterEvent(QGraphicsSceneHoverEvent *event)
{
	m_zoomTimeLine.setDirection(QTimeLine::Forward);

	if (m_z != 1) 
	{
		m_z = 1;
		updateZvalue();
	}

	if (m_zoomTimeLine.state() == QTimeLine::NotRunning)
	{
		m_zoomTimeLine.setCurve(CLAnimationTimeLine::CLAnimationCurve::SLOW_END);
		//m_zoomTimeLine.setCurve(CLAnimationTimeLine::CLAnimationCurve::SLOW_START);
		m_zoomTimeLine.start();
	}

}

void VideoWindow::hoverLeaveEvent(QGraphicsSceneHoverEvent *event)
{
	m_zoomTimeLine.setDirection(QTimeLine::Backward);
	if (m_z != 0)
	{
		m_z = 0;
		//updateZvalue();
	}

	if (m_zoomTimeLine.state() == QTimeLine::NotRunning)
	{
		m_zoomTimeLine.setCurve(CLAnimationTimeLine::CLAnimationCurve::SLOW_START);
		m_zoomTimeLine.start();
	}

}


void VideoWindow::setMouseMoveZoom(int zoom)
{
	QPointF center = boundingRect().center();

	QTransform trans;
	trans.translate(center.x(), center.y());
	trans.scale(1 + zoom/ 3300.0, 1 + zoom/ 3300.0);
	trans.translate(-center.x(), -center.y());
	setTransform(trans);

}

void VideoWindow::updateZvalue()
{
	setZValue(qreal(m_z));
}
