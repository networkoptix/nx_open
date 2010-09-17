#include "uvideo_wnd.h"
#include <QPainter>
#include "../graphicsview.h"
#include "camera\camera.h"
#include "device/device.h"


extern int item_select_duration;
extern qreal selected_item_zoom;
extern int item_hoverevent_duration;

static const double Pi = 3.14159265358979323846264338327950288419717;
static double TwoPi = 2.0 * Pi;


VideoWindow::VideoWindow(GraphicsView* view, const CLDeviceVideoLayout* layout, int max_width, int max_height):
CLVideoWindow(layout, max_width, max_height),
m_selected(false),
m_z(0),
m_animationTransform(this),
m_view(view),
m_draw_rotation_helper(false)
{
	setAcceptsHoverEvents(true);

}


void VideoWindow::setSelected(bool sel, bool animate, int delay)
{
	m_selected = sel;

	if (!m_selected)
		setFullScreen(false);

	
	if (m_selected)
		getVideoCam()->setQuality(CLStreamreader::CLSHighest, true);
	else
		getVideoCam()->setQuality(CLStreamreader::CLSNormal, false);

	if (!animate)
		return;

	if (m_selected)
	{
		
		m_animationTransform.zoom_abs(selected_item_zoom, item_select_duration - 25, delay);
		setZValue(2);
	}
	else
	{
		m_animationTransform.zoom_abs(1.0, item_hoverevent_duration, delay);
		setZValue(0);
	}
}


bool VideoWindow::isSelected() const
{
	return m_selected;
}

void VideoWindow::zoom_abs(qreal z, int duration, int delay )
{
	m_animationTransform.zoom_abs(z, duration);
}

qreal VideoWindow::getZoom() const
{
	return m_animationTransform.current_zoom();
}

qreal VideoWindow::getRotation() const
{
	return m_animationTransform.current_zrotation();
}

void VideoWindow::setRotation(qreal angle)
{
	m_animationTransform.z_rotate_abs(QPointF(), angle, 0);
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

void VideoWindow::drawRotationHelper(bool val)
{
	m_draw_rotation_helper = val;
}

void VideoWindow::setRotationPoint(QPointF point1, QPointF point2)
{
	m_rotation_center = point1;
	m_rotation_hand = point2;
}


//================================================

void VideoWindow::drawStuff(QPainter* painter)
{
	CLVideoWindow::drawStuff(painter);

	if (m_selected)
		drawSelection(painter);

	if (m_draw_rotation_helper)
		drawRotationHelper(painter);


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
			m_animationTransform.zoom_abs(1.11, item_hoverevent_duration);

		}
	}


	CLDevice* dev = getVideoCam()->getDevice();
	if (!dev->checkDeviceTypeFlag(CLDevice::SINGLE_SHOT))
	{
		setShowImagesize(true);
	}

	if (!dev->checkDeviceTypeFlag(CLDevice::ARCHIVE) && !dev->checkDeviceTypeFlag(CLDevice::SINGLE_SHOT))
	{
		showFPS(true);
		setShowInfoText(true);
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
			m_animationTransform.zoom_abs(1.0, item_hoverevent_duration);
		}

			setShowImagesize(false);
			showFPS(false);
			setShowInfoText(false);
	}

}

void VideoWindow::drawSelection(QPainter* painter)
{
}


void VideoWindow::drawRotationHelper(QPainter* painter)
{
	static QColor color1(255, 0, 0, 250);
	static QColor color2(255, 0, 0, 100);

	static const int r = 30;
	static const int penWidth = 6;
	static const int p_line_len = 220;
	static const int arrowSize = 30;



	QRadialGradient gradient(m_rotation_center, r);
	gradient.setColorAt(0, color1);
	gradient.setColorAt(1, color2);

	painter->save();

	painter->setPen(QPen(color2, 0, Qt::SolidLine));


	
	painter->setBrush(gradient);
	painter->drawEllipse(m_rotation_center, r, r);

	painter->setPen(QPen(color2, penWidth, Qt::SolidLine));
	painter->drawLine(m_rotation_center,m_rotation_hand);

	// building new line
	QLineF line(m_rotation_hand, m_rotation_center);
	QLineF line_p = line.unitVector().normalVector();
	line_p.setLength(p_line_len/2);

	line_p = QLineF(line_p.p2(),line_p.p1());
	line_p.setLength(p_line_len);

	

	painter->drawLine(line_p);




	double angle = ::acos(line_p.dx() / line_p.length());
	if (line_p.dy() >= 0)
		angle = TwoPi - angle;

	qreal s = 2.5;

	QPointF sourceArrowP1 = line_p.p1() + QPointF(sin(angle + Pi / s) * arrowSize,
		cos(angle + Pi / s) * arrowSize);
	QPointF sourceArrowP2 = line_p.p1() + QPointF(sin(angle + Pi - Pi / s) * arrowSize,
		cos(angle + Pi - Pi / s) * arrowSize);   
	QPointF destArrowP1 = line_p.p2() + QPointF(sin(angle - Pi / s) * arrowSize,
		cos(angle - Pi / s) * arrowSize);
	QPointF destArrowP2 = line_p.p2() + QPointF(sin(angle - Pi + Pi / s) * arrowSize,
		cos(angle - Pi + Pi / s) * arrowSize);

	painter->setBrush(color2);
	painter->drawPolygon(QPolygonF() << line_p.p1() << sourceArrowP1 << sourceArrowP2);
	painter->drawPolygon(QPolygonF() << line_p.p2() << destArrowP1 << destArrowP2);        

	painter->restore();
	/**/

}