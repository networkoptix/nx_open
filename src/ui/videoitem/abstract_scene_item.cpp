#include "abstract_scene_item.h"
#include "ui\graphicsview.h"


extern int item_select_duration;
extern qreal selected_item_zoom;
extern int item_hoverevent_duration;

#define SHADOW_SIZE 100


CLAbstractSceneItem::CLAbstractSceneItem(GraphicsView* view, int max_width, int max_height, 
										 QString name, QObject* handler):
m_max_width(max_width),
m_max_height(max_height),
m_animationTransform(this),
m_view(view),
m_type(BUTTON),
mName(name),
m_selected(false),
m_fullscreen(false),
m_arranged(true),
m_mouse_over(false)
{
	setAcceptsHoverEvents(true);

	setZValue(1.0);

	setFlag(QGraphicsItem::ItemIsFocusable);

	setFlag(QGraphicsItem::ItemIgnoresParentOpacity, true);

	if (handler)
		connect(this, SIGNAL(onPressed(QString)), handler, SLOT(onItemPressed(QString)));
}

CLAbstractSceneItem::~CLAbstractSceneItem()
{

}

CLAbstractSceneItem::CLSceneItemType CLAbstractSceneItem::getType() const
{
	return m_type;
}

void CLAbstractSceneItem::setType(CLSceneItemType t)
{
	m_type = t;
}


void CLAbstractSceneItem::setMaxSize(QSize size)
{
	m_max_width = size.width();
	m_max_height = size.height();
}

void CLAbstractSceneItem::before_destroy()
{

}

QRectF CLAbstractSceneItem::boundingRect() const
{
	return QRectF(0,0,width(),height()); 
}

int CLAbstractSceneItem::height() const
{
	return m_max_height;
}

int CLAbstractSceneItem::width() const
{
	return m_max_width;
}

void CLAbstractSceneItem::setSelected(bool sel, bool animate , int delay )
{
	m_selected = sel;

	if (!m_selected)
		setFullScreen(false);

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

bool CLAbstractSceneItem::isSelected() const
{
	return m_selected;
}

void CLAbstractSceneItem::zoom_abs(qreal z, int duration, int delay)
{
	m_animationTransform.zoom_abs(z, duration);
}

void CLAbstractSceneItem::z_rotate_delta(QPointF center, qreal angle, int duration)
{
	m_animationTransform.z_rotate_delta(center, angle, duration);
}

void CLAbstractSceneItem::z_rotate_abs(QPointF center, qreal angle, int duration)
{
	m_animationTransform.z_rotate_abs(center, angle, duration);
}

qreal CLAbstractSceneItem::getZoom() const
{
	return m_animationTransform.current_zoom();
}

qreal CLAbstractSceneItem::getRotation() const
{
	return m_animationTransform.current_zrotation();
}

void CLAbstractSceneItem::setRotation(qreal angle)
{
	m_animationTransform.z_rotate_abs(QPointF(), angle, 0);
}

void CLAbstractSceneItem::stop_animation()
{
	m_animationTransform.stop();
}

void CLAbstractSceneItem::setFullScreen(bool full)
{
	m_fullscreen = full;
}

bool CLAbstractSceneItem::isFullScreen() const
{
	return m_fullscreen;
}

void CLAbstractSceneItem::setArranged(bool arr)
{
	m_arranged = arr;
}

bool CLAbstractSceneItem::isArranged() const
{
	return m_selected;
}


void CLAbstractSceneItem::hoverEnterEvent(QGraphicsSceneHoverEvent *event)
{
	m_mouse_over = true;

	if (m_view->getZoom() < 0.25)
	{
		if (m_z != 1) 
		{
			m_z = 1;
			setZValue(m_z);
			m_animationTransform.zoom_abs(1.11, item_hoverevent_duration);

		}
	}

}

void CLAbstractSceneItem::hoverLeaveEvent(QGraphicsSceneHoverEvent *event)
{

	m_mouse_over = false;

	//if (m_view->getSelectedWnd()!=this) //todo
	{
		if (m_z != 0)
		{
			m_z = 0;
			setZValue(m_z);
			m_animationTransform.zoom_abs(1.0, item_hoverevent_duration);
		}
	}
}

void CLAbstractSceneItem::mouseReleaseEvent( QGraphicsSceneMouseEvent * event )
{
	emit onPressed(mName);
}

void CLAbstractSceneItem::drawShadow(QPainter* painter)
{
	QColor shadow_color(0, 0, 0, 128);
	QRect rect1(width(), SHADOW_SIZE, SHADOW_SIZE, height());
	QRect rect2(SHADOW_SIZE, height(), width()-SHADOW_SIZE, SHADOW_SIZE);
	painter->fillRect(rect1, shadow_color);
	painter->fillRect(rect2, shadow_color);

	if (getType()==IMAGE)
	{
		const int fr_w = 20;
		painter->setPen(QPen(QColor(150,150,150,200),  fr_w, Qt::SolidLine));
		painter->drawRect(-fr_w/2,-fr_w/2,width()+fr_w,height()+fr_w);
	}
}