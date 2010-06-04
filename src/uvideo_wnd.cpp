#include "uvideo_wnd.h"


#include <QPainter>

VideoWindow::VideoWindow(const CLDeviceVideoLayout* layout, int max_width, int max_height):
CLVideoWindow(layout, max_width, max_height),
m_selected(false),
m_z(0),
m_zoom(this)
{
	setAcceptsHoverEvents(true);
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
	if (m_z != 1) 
	{
		m_z = 1;
		setZValue(m_z);
		m_zoom.zoom(400);

	}
}

void VideoWindow::hoverLeaveEvent(QGraphicsSceneHoverEvent *event)
{
	if (m_z != 0)
	{
		m_z = 0;
		setZValue(m_z);
		m_zoom.zoom(0);
	}

}


