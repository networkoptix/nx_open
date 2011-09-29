#include "abstract_scene_item.h"

#include <QtGui/QGraphicsScene>
#include <QtGui/QGraphicsSceneEvent>
#include <QtGui/QGraphicsView>

#include "video_wnd_item.h"

static const qreal global_base_scene_z_level = 1.0; // ###

CLAbstractSceneItem::CLAbstractSceneItem(int max_width, int max_height, const QString &name)
    : CLAbstractSubItemContainer(0),
    m_arranged(true),
    m_fullscreen(false),
    m_selected(false),
    m_mouse_over(false),
    m_zoomOnhover(true),
    m_max_width(max_width),
    m_max_height(max_height),
    m_type(BUTTON),
    mName(name),
    m_editable(false),
    m_needUpdate(false),
    m_can_be_droped(false)
{
    setAcceptsHoverEvents(true);

    setZValue(global_base_scene_z_level);

    //setFlag(ItemIsSelectable, true);
    //setFlag(QGraphicsItem::ItemIsFocusable);
    //setFlag(QGraphicsItem::ItemIgnoresParentOpacity);
}

CLAbstractSceneItem::~CLAbstractSceneItem()
{
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
    return m_editable;
}

void CLAbstractSceneItem::setEditable(bool editable)
{
    m_editable = editable;
}

bool CLAbstractSceneItem::isZoomable() const
{
    return false;
}

CLVideoWindowItem* CLAbstractSceneItem::toVideoItem() const
{
    return m_type == VIDEO ? static_cast<CLVideoWindowItem *>(const_cast<CLAbstractSceneItem*>(this)) : 0;
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
    onResize();
}

QSize CLAbstractSceneItem::getMaxSize() const
{
    return QSize(m_max_width, m_max_height);
}

QSize CLAbstractSceneItem::onScreenSize() const
{
    return scene()->views().at(0)->mapFromScene(sceneBoundingRect()).boundingRect().size();
}

QRectF CLAbstractSceneItem::boundingRect() const
{
    return QRectF(0, 0, m_max_width, m_max_height);
}

int CLAbstractSceneItem::width() const
{
    return m_max_width;
}

int CLAbstractSceneItem::height() const
{
    return m_max_height;
}

void CLAbstractSceneItem::setItemSelected(bool sel, bool animate , int delay )
{
    if (!m_selected && sel)
        emit onSelected(this);

    m_selected = sel;

    if (!m_selected)
        setFullScreen(false);

    if (m_selected)
        setZValue(global_base_scene_z_level+1);
    else
        setZValue(global_base_scene_z_level);
}

bool CLAbstractSceneItem::isItemSelected() const
{
    return m_selected;
}

void CLAbstractSceneItem::setFullScreen(bool full)
{
    m_fullscreen = full;
    if (m_fullscreen)
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

void CLAbstractSceneItem::setOriginalPos(const QPointF &pos)
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

void CLAbstractSceneItem::hoverEnterEvent(QGraphicsSceneHoverEvent * /*event*/)
{
    m_mouse_over = true;
}

void CLAbstractSceneItem::hoverLeaveEvent(QGraphicsSceneHoverEvent * /*event*/)
{
    m_mouse_over = false;
}

void CLAbstractSceneItem::mousePressEvent ( QGraphicsSceneMouseEvent * event )
{
    if (!isSelected()) // to avoid unselect all; see the code of QT
        QGraphicsItem::mousePressEvent(event);

    event->accept();
}

void CLAbstractSceneItem::mouseDoubleClickEvent(QGraphicsSceneMouseEvent * /*event*/)
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
