#include "uvideo_wnd.h"
#include <QPainter>
#include "graphicsview.h"
#include "camera\camera.h"


extern int item_select_duration;
extern qreal selected_item_zoom;
extern int item_hoverevent_duration;

VideoWindow::VideoWindow(GraphicsView* view, const CLDeviceVideoLayout* layout, int max_width, int max_height):
CLVideoWindow(layout, max_width, max_height),
m_selected(false),
m_fullscreen(false),
m_z(0),
m_animationTransform(this),
m_view(view)
{
	setAcceptsHoverEvents(true);

}


void VideoWindow::setSelected(bool sel, bool animate )
{
	m_selected = sel;

	if (!m_selected)
		m_fullscreen = false;

	
	if (m_selected)
		getVideoCam()->setQuality(CLStreamreader::CLSHigh, true);
	else
		getVideoCam()->setQuality(CLStreamreader::CLSNormal, false);

	if (!animate)
		return;

	if (m_selected)
	{
		
		m_animationTransform.zoom(selected_item_zoom, item_select_duration - 25);
		setZValue(1);
	}
	else
	{
		m_animationTransform.zoom(1.0, item_hoverevent_duration);
		setZValue(0);
	}
}

void VideoWindow::setFullScreen(bool full)
{
	m_fullscreen = full;
}

bool VideoWindow::isFullScreen() const
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


void VideoWindow::zoom_abs(qreal z, int duration)
{
	m_animationTransform.zoom(z, duration);
}

void VideoWindow::z_rotate_delta(QPointF center, qreal angle, int duration)
{
	m_animationTransform.z_rotate_delta(center, angle, duration);
}

void VideoWindow::z_rotate_abs(QPointF center, qreal angle, int duration)
{
	m_animationTransform.z_rotate_abs(center, angle, duration);
}

void VideoWindow::stop_animation()
{
	m_animationTransform.stop();
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
			m_animationTransform.zoom(1.11, item_hoverevent_duration);

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
			m_animationTransform.zoom(1.0, item_hoverevent_duration);
		}
	}

}


