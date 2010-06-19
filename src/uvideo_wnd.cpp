#include "uvideo_wnd.h"
#include <QPainter>
#include "graphicsview.h"

extern int item_select_duration;
extern qreal selected_item_zoom;

VideoWindow::VideoWindow(GraphicsView* view, const CLDeviceVideoLayout* layout, int max_width, int max_height):
CLVideoWindow(layout, max_width, max_height),
m_selected(false),
m_z(0),
m_zoom(this),
m_view(view)
{
	setAcceptsHoverEvents(true);
}


void VideoWindow::setSelected(bool sel)
{
	m_selected = sel;

	if (m_selected)
	{
		m_zoom.setDuration(item_select_duration - 25);
		m_zoom.zoom(selected_item_zoom);
		setZValue(1);
	}
	else
	{
		m_zoom.restoreDefaultDuration();
		m_zoom.zoom(1.0);
		setZValue(0);
	}
}

void VideoWindow::setFullScreen(bool full)
{
	m_fullscreen = full;
}

bool VideoWindow::getFullScreen() const
{
	return m_fullscreen;
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


void VideoWindow::zoom_abs(qreal z, bool instantly)
{
	m_zoom.zoom(z, instantly);
}


//===========================================

void VideoWindow::hoverEnterEvent(QGraphicsSceneHoverEvent *event)
{
	if (m_view->getZoom() < 0.25)
	{
		if (m_z != 1) 
		{
			m_z = 1;
			setZValue(m_z);
			m_zoom.zoom(1.11);

		}
	}

}

void VideoWindow::hoverLeaveEvent(QGraphicsSceneHoverEvent *event)
{
	if (m_view->getSelectedWnd()!=this)
	{
		if (m_z != 0)
		{
			m_z = 0;
			setZValue(m_z);
			m_zoom.zoom(1.0);
		}
	}

}


