#include "uvideo_wnd.h"
#include <QPainter>

extern int item_select_duration;

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

	if (m_selected)
	{
		m_zoom.setDuration(item_select_duration + 100);
		m_zoom.zoom(2500);
		setZValue(1);
	}
	else
	{
		m_zoom.restoreDefaultDuration();
		m_zoom.zoom(0);
		setZValue(0);
	}
}

bool VideoWindow::isSelected() const
{
	return m_selected;
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

void VideoWindow::zoom_abs(int z)
{
	m_zoom.zoom(z);
}

//===========================================

void VideoWindow::hoverEnterEvent(QGraphicsSceneHoverEvent *event)
{
	if (m_z != 1) 
	{
		m_z = 1;
		//setZValue(m_z);
		//m_zoom.zoom(400);

	}
}

void VideoWindow::hoverLeaveEvent(QGraphicsSceneHoverEvent *event)
{
	if (m_z != 0)
	{
		m_z = 0;
		//setZValue(m_z);
		//m_zoom.zoom(0);
	}

}


