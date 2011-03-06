#include "abstract_scene_item.h"
#include "ui\graphicsview.h"
#include "video_wnd_item.h"

#include <math.h>
#include "settings.h"
#include "subitem\abstract_image_sub_item.h"
#include "subitem\recording_sign_item.h"
#include "base\log.h"

static const double Pi = 3.14159265358979323846264338327950288419717;
static double TwoPi = 2.0 * Pi;

extern int item_select_duration;
extern qreal selected_item_zoom;
extern int item_hoverevent_duration;

#define SHADOW_SIZE 100

extern int global_opacity_change_period;

CLAbstractSceneItem::CLAbstractSceneItem(GraphicsView* view, int max_width, int max_height, 
										 QString name):
CLAbstractSubItemContainer(0),
m_max_width(max_width),
m_max_height(max_height),
m_animationTransform(this),
m_view(view),
m_type(BUTTON),
mName(name),
m_selected(false),
m_fullscreen(false),
m_arranged(true),
m_mouse_over(false),
m_draw_rotation_helper(false),
m_zoomOnhover(true),
m_needUpdate(false),
mEditable(false),
m_rotation_center(0,0),
m_complicatedItem(0),
m_can_be_droped(false),
m_steadyMode(false),
m_steady_animation(0),
m_steady_black_color(0)
{
	setAcceptsHoverEvents(true);

	setZValue(global_base_scene_z_level);

	//setFlag(QGraphicsItem::ItemIsFocusable);

	//setFlag(QGraphicsItem::ItemIgnoresParentOpacity, true);

	//setFlag(ItemIsSelectable, true);

}

CLAbstractSceneItem::~CLAbstractSceneItem()
{
	stop_animation();
}

QString CLAbstractSceneItem::getName() const
{
	return mName;
}

void CLAbstractSceneItem::setName(const QString& name)
{
	mName = name;
}

bool CLAbstractSceneItem::isEtitable() const
{
	return mEditable;
}

void CLAbstractSceneItem::setEditable(bool editable)
{
	mEditable = editable;
}

bool CLAbstractSceneItem::isZoomable() const
{
	return false;
}

CLVideoWindowItem* CLAbstractSceneItem::toVideoItem() const
{
	if (m_type!=VIDEO)
		return 0;

	return static_cast<CLVideoWindowItem*>(   const_cast<CLAbstractSceneItem*>(this)  );
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

QSize CLAbstractSceneItem::getMaxSize() const
{
	return QSize(m_max_width, m_max_height);
}

QSize CLAbstractSceneItem::onScreenSize() const
{
	return m_view->mapFromScene(sceneBoundingRect()).boundingRect().size();
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

void CLAbstractSceneItem::setItemSelected(bool sel, bool animate , int delay )
{
	if (!m_selected && sel)
		emit onSelected(this);

	m_selected = sel;

	if (!m_selected)
		setFullScreen(false);

	if (!animate)
		return;

	if (m_selected)
	{

		m_animationTransform.zoom_abs(selected_item_zoom, item_select_duration - 25, delay);
		setZValue(global_base_scene_z_level+1);
	}
	else
	{
		m_animationTransform.zoom_abs(1.0, item_hoverevent_duration, delay);
		setZValue(global_base_scene_z_level);
	}

}

bool CLAbstractSceneItem::isItemSelected() const
{
	return m_selected;
}

void CLAbstractSceneItem::zoom_abs(qreal z, int duration, int delay)
{
	m_animationTransform.zoom_abs(z, duration, 0);
}

void CLAbstractSceneItem::z_rotate_delta(QPointF center, qreal angle, int duration, int delay)
{
	m_animationTransform.z_rotate_delta(center, angle, duration, delay);
}

void CLAbstractSceneItem::z_rotate_abs(QPointF center, qreal angle, int duration, int delay)
{
	m_animationTransform.z_rotate_abs(center, angle, duration, delay);
}

qreal CLAbstractSceneItem::getZoom() const
{
	return m_animationTransform.current_zoom();
}

CLAbstractComplicatedItem* CLAbstractSceneItem::getComplicatedItem() const
{
	return m_complicatedItem;
}

void CLAbstractSceneItem::setComplicatedItem(CLAbstractComplicatedItem* complicatedItem)
{
	m_complicatedItem = complicatedItem;
}

qreal CLAbstractSceneItem::getRotation() const
{
	return m_animationTransform.current_zrotation();
}

void CLAbstractSceneItem::setRotation(qreal angle)
{
	m_animationTransform.z_rotate_abs(m_rotation_center, angle, 0, 0);
}

void CLAbstractSceneItem::stop_animation()
{
	m_animationTransform.stopAnimation();

    QList<QGraphicsItem *> childrenLst = childItems();
    foreach(QGraphicsItem * item, childrenLst)
    {
        CLAbstractSubItem* sub_item = static_cast<CLAbstractSubItem*>(item);
        sub_item->stopAnimation();
    }

}

void CLAbstractSceneItem::setFullScreen(bool full)
{
	m_fullscreen = full;

    if (!m_fullscreen)
        goToSteadyMode(false, true);

	if (m_fullscreen )
		emit onFullScreen(this);
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
	return m_arranged;
}

void CLAbstractSceneItem::setCanDrop(bool val)
{
	m_can_be_droped = val;
}

QPointF CLAbstractSceneItem::getOriginalPos() const
{
	return m_originalPos;

}

void CLAbstractSceneItem::setOroginalPos(QPointF pos)
{
	m_originalPos = pos;
}

bool CLAbstractSceneItem::getOriginallyArranged() const
{
	return m_originallyArranged;
}

void CLAbstractSceneItem::setOriginallyArranged(bool val)
{
	m_originallyArranged = val;
}

void CLAbstractSceneItem::hoverEnterEvent(QGraphicsSceneHoverEvent *event)
{
	m_mouse_over = true;

	/*/
	if (m_view->getSelectedItem()!=this &&(m_view->getZoom() < 0.25 || m_zoomOnhover))
	{
		if (m_z != 1) 
		{
			m_z = 1;
			setZValue(m_z);
			m_animationTransform.zoom_abs(1.11, item_hoverevent_duration, 0);
		}
	}
	/**/

}

void CLAbstractSceneItem::hoverLeaveEvent(QGraphicsSceneHoverEvent *event)
{

	m_mouse_over = false;

	/*/
	if (m_view->getSelectedItem()!=this) 
	{
		if (m_z != 0)
		{
			m_z = 0;
			setZValue(m_z);
			m_animationTransform.zoom_abs(1.0, item_hoverevent_duration, 0);
		}
	}
	/**/
}

void CLAbstractSceneItem::mousePressEvent ( QGraphicsSceneMouseEvent * event )
{
	if (!isSelected()) // to avoid unselect all; see the code of QT
		QGraphicsItem::mousePressEvent(event);

	event->accept();
}

void CLAbstractSceneItem::mouseDoubleClickEvent( QGraphicsSceneMouseEvent * event )
{
	emit onDoubleClick(this);
}

void CLAbstractSceneItem::mouseReleaseEvent( QGraphicsSceneMouseEvent * event )
{
	if (event->button()==Qt::LeftButton)
		emit onPressed(this);

	if (!isSelected())
		QGraphicsItem::mouseReleaseEvent(event);
}

void CLAbstractSceneItem::drawSteadyWall(QPainter* painter)
{
    if (!m_steadyMode)
        return; // do not need to draw anything in this mode

    QColor color(0,0,0,m_steady_black_color);

    QRect rect(0, 0, width(), height());

    int maxd = qMax(width(), height());
    rect.adjust(-maxd, -maxd, maxd, maxd);
    painter->fillRect(rect, color);

}

void CLAbstractSceneItem::drawShadow(QPainter* painter)
{
    if (m_steadyMode)
        return; // do not need to draw anything in this mode

	QRect rect1(width(), SHADOW_SIZE, SHADOW_SIZE, height());
	QRect rect2(SHADOW_SIZE, height(), width()-SHADOW_SIZE, SHADOW_SIZE);
	painter->fillRect(rect1, global_shadow_color);
	painter->fillRect(rect2, global_shadow_color);

	if (getType()==IMAGE || getType()==VIDEO)
	{
		const int fr_w = 20;
		painter->setPen(QPen(QColor(150,150,150,200),  fr_w, Qt::SolidLine));
		painter->drawRect(-fr_w/2,-fr_w/2,width()+fr_w,height()+fr_w);
	}

}

void CLAbstractSceneItem::drawRotationHelper(bool val)
{
	m_draw_rotation_helper = val;
}

void CLAbstractSceneItem::setRotationPointCenter(QPointF center)
{

	qreal angle = getRotation();

	qreal cos_a = cos(angle*Pi/180);
	qreal sin_a = sin(angle*Pi/180);

	qreal x_old = (-m_rotation_center.x())*cos_a - (-m_rotation_center.y())*sin_a; // pos of the item after rotation in center of rotation coordinates 
	qreal y_old = (-m_rotation_center.x())*sin_a +  (-m_rotation_center.y())*cos_a;
	x_old += m_rotation_center.x(); // pos of the item after rotation in item coordinates 
	y_old += m_rotation_center.y();

	qreal x_new = (-center.x())*cos_a - (-center.y())*sin_a; // pos of the item after rotation in center of rotation coordinates ( around nex center )
	qreal y_new = (-center.x())*sin_a +  (-center.y())*cos_a;
	x_new += center.x(); // pos of the item after rotation in item coordinates ( around nex center )
	y_new += center.y();

	QPointF delta(x_old - x_new, y_old - y_new);

	setPos(pos() + delta);
	/**/

	m_rotation_center  = center;
}

QPointF CLAbstractSceneItem::getRotationPointCenter() const
{
	return m_rotation_center;
}

void CLAbstractSceneItem::setRotationPointHand(QPointF point)
{

	m_rotation_hand = point;
}

void CLAbstractSceneItem::drawRotationHelper(QPainter* painter)
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

void CLAbstractSceneItem::stopSteadyAnimation()
{
    if (m_steady_animation)
    {
        if (steadyBlackColor()==0)
            m_steadyMode = false;
        m_steady_animation->stop();
        delete m_steady_animation;
        m_steady_animation = 0;
    }
}

void CLAbstractSceneItem::goToSteadyMode(bool steady, bool instant)
{
    if (steady)
    {
        m_steadyMode = true;

        QList<QGraphicsItem *> childrenLst = childItems();
        foreach(QGraphicsItem * item, childrenLst)
        {
            CLAbstractSubItem* sub_item = static_cast<CLAbstractSubItem*>(item);
            sub_item->hide(global_opacity_change_period*(!instant));
        }

        if (instant)
        {
            setSteadyBlackColor(255);
            return;
        }

        stopSteadyAnimation();

        m_steady_animation = new QPropertyAnimation(this, "steadycolor");
        m_steady_animation->setDuration(global_opacity_change_period);
        m_steady_animation->setStartValue(steadyBlackColor());
        m_steady_animation->setEndValue(255);
        m_steady_animation->start();
        connect(m_steady_animation, SIGNAL(finished ()), this, SLOT(stopSteadyAnimation()));

        return;
    }

    // escape steady mode
    QList<QGraphicsItem *> childrenLst = childItems();
    foreach(QGraphicsItem * item, childrenLst)
    {
        CLAbstractSubItem* sub_item = static_cast<CLAbstractSubItem*>(item);
        sub_item->show(global_opacity_change_period*(!instant));
    }

    if (instant)
    {
        stopSteadyAnimation();
        setSteadyBlackColor(255);
        m_steadyMode = false;
        return;
    }

    stopSteadyAnimation();

    m_steady_animation = new QPropertyAnimation(this, "steadycolor");
    m_steady_animation->setDuration(global_opacity_change_period);
    m_steady_animation->setStartValue(steadyBlackColor());
    m_steady_animation->setEndValue(0);
    m_steady_animation->start();
    connect(m_steady_animation, SIGNAL(finished ()), this, SLOT(stopSteadyAnimation()));

}

int CLAbstractSceneItem::steadyBlackColor() const
{
    return m_steady_black_color;
}

void CLAbstractSceneItem::setSteadyBlackColor(int color) 
{
    m_steady_black_color = color;
    update();
}
