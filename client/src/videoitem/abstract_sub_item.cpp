#include "abstract_sub_item.h"

#include <QtGui/QGraphicsScene>
#include <QtGui/QGraphicsSceneEvent>
#include <QtGui/QGraphicsView>
#include <QtGui/QPainter>

#include "abstract_scene_item.h"

/*#include "abstract_image_sub_item.h"
#include "recording_sign_item.h"*/

CLAbstractSubItemContainer::CLAbstractSubItemContainer(QGraphicsItem *parent)
    : QObject(), QGraphicsItem(parent)
{
}

CLAbstractSubItemContainer::~CLAbstractSubItemContainer()
{
}

QList<CLAbstractSubItem*> CLAbstractSubItemContainer::subItemList() const
{
    return m_subItems;
}

void CLAbstractSubItemContainer::addSubItem(CLAbstractSubItem *item)
{
    if (!m_subItems.contains(item))
        m_subItems.push_back(item);
}

void CLAbstractSubItemContainer::removeSubItem(CLAbstractSubItem *item)
{
    m_subItems.removeOne(item);
}

bool CLAbstractSubItemContainer::addSubItem(CLSubItemType type)
{
    QPointF pos = getBestSubItemPos(type);
    if (pos.x()<-1000 && pos.y()<-1000)//position undefined
        return false;

    CLAbstractSubItem* item = 0;

    switch(type)
    {
/*    case CloseSubItem:
        item = new CLImgSubItem(this, QLatin1String(":/skin/close3.png") ,CloseSubItem, global_decoration_opacity, global_decoration_max_opacity, 256, 256);
        break;

    case RecordingSubItem:
        item = new CLRecordingSignItem(this);
        break;*/

    default:
        return false;
    }

    item->setPos(pos);

    addSubItem(item);

    return true;
}

void CLAbstractSubItemContainer::removeSubItem(CLSubItemType type)
{
    foreach(CLAbstractSubItem* sub_item, m_subItems)
    {
        if (sub_item->getType()==type)
        {
            removeSubItem(sub_item);
            scene()->removeItem(sub_item);
            delete sub_item;
            break;
        }

    }
}

QPointF CLAbstractSubItemContainer::getBestSubItemPos(CLSubItemType /*type*/)
{
    return QPointF(-1001,-1001);
}

void CLAbstractSubItemContainer::onResize()
{
    foreach(CLAbstractSubItem* sub_item, m_subItems)
    {
        QPointF pos = getBestSubItemPos(sub_item->getType());
        sub_item->setPos(pos);
        sub_item->onResize();
    }
}

void CLAbstractSubItemContainer::onSubItemPressed(CLAbstractSubItem* subitem)
{
    CLSubItemType type = subitem->getType();

    switch(type)
    {
    case CloseSubItem:
        emit onClose(this);
        break;

    default:
        break;
    }
}

void CLAbstractSubItemContainer::addToEevntTransparetList(QGraphicsItem* item)
{
    m_eventtransperent_list.push_back(item);
}

bool CLAbstractSubItemContainer::isInEventTransparetList(QGraphicsItem* item) const
{
    return m_eventtransperent_list.contains(item);
}

//==============================================================================

CLAbstractUnmovedItem::CLAbstractUnmovedItem(QString name, QGraphicsItem* parent) :
    CLAbstractSubItemContainer(parent),
    m_name(name),
    m_view(0),
    m_underMouse(false)
{
    setFlag(QGraphicsItem::ItemIgnoresTransformations);
    //setFlag(QGraphicsItem::ItemIsMovable);

    setAcceptsHoverEvents(true);
}

CLAbstractUnmovedItem::~CLAbstractUnmovedItem()
{
}

void CLAbstractUnmovedItem::hideIfNeeded(int duration)
{
    hide(duration);
}

bool CLAbstractUnmovedItem::preferNonSteadyMode() const
{
    //return isUnderMouse(); //qt bug 18797 When setting the flag ItemIgnoresTransformations for an item, it will receive mouse events as if it was transformed by the view.
    return m_underMouse;
}

void CLAbstractUnmovedItem::hoverEnterEvent(QGraphicsSceneHoverEvent *event)
{
    Q_UNUSED(event);
    m_underMouse = true;
}

void CLAbstractUnmovedItem::hoverLeaveEvent(QGraphicsSceneHoverEvent *event)
{
    Q_UNUSED(event);
    m_underMouse = false;
}

void CLAbstractUnmovedItem::setStaticPos(const QPoint &p)
{
    m_pos = p;
}

void CLAbstractUnmovedItem::adjust()
{
    if (!m_view)
        m_view = scene()->views().at(0);

    setPos(m_view->mapToScene(m_pos));
}

QString CLAbstractUnmovedItem::getName() const
{
    return m_name;
}

//==============================================================================

CLAbstractUnMovedOpacityItem::CLAbstractUnMovedOpacityItem(QString name, QGraphicsItem* parent)
    : CLAbstractUnmovedItem(name, parent)
{
}

CLAbstractUnMovedOpacityItem::~CLAbstractUnMovedOpacityItem()
{
}

void CLAbstractUnMovedOpacityItem::changeOpacity(qreal new_opacity, int duration_ms)
{
    setOpacity(new_opacity);
}

//==============================================================================

CLUnMovedInteractiveOpacityItem::CLUnMovedInteractiveOpacityItem(QString name, QGraphicsItem* parent, qreal normal_opacity, qreal active_opacity):
    CLAbstractUnMovedOpacityItem(name, parent),
    m_normal_opacity(normal_opacity),
    m_active_opacity(active_opacity)
{
    setOpacity(m_normal_opacity);
}

CLUnMovedInteractiveOpacityItem::~CLUnMovedInteractiveOpacityItem()
{
}

void CLUnMovedInteractiveOpacityItem::hoverEnterEvent(QGraphicsSceneHoverEvent *event)
{
    CLAbstractUnMovedOpacityItem::hoverEnterEvent(event);

    changeOpacity(m_active_opacity, 0);
}

void CLUnMovedInteractiveOpacityItem::hoverLeaveEvent(QGraphicsSceneHoverEvent *event)
{
    CLAbstractUnMovedOpacityItem::hoverLeaveEvent(event);

    changeOpacity(m_normal_opacity, 0);
}

void CLUnMovedInteractiveOpacityItem::hide(int duration)
{
    changeOpacity(0, duration);
}

void CLUnMovedInteractiveOpacityItem::show(int duration)
{
    changeOpacity(m_normal_opacity, duration);
}

//==============================================================================

CLAbstractSubItem::CLAbstractSubItem(CLAbstractSubItemContainer* parent, qreal normal_opacity, qreal active_opacity)
    : CLUnMovedInteractiveOpacityItem(QString(), static_cast<QGraphicsItem*>(parent), normal_opacity, active_opacity),
      m_parent(parent)
{
    setFlag(QGraphicsItem::ItemIgnoresTransformations, false); // at any momrnt this item can becom umoved; but by default disable this flag

    setAcceptsHoverEvents(true);
    //setZValue(parent->zvalue() + 1);

    connect(this, SIGNAL(onPressed(CLAbstractSubItem*)), static_cast<QObject*>(parent), SLOT(onSubItemPressed(CLAbstractSubItem*)) );
    //onSubItemPressed
}

CLAbstractSubItem::~CLAbstractSubItem()
{
}

CLSubItemType CLAbstractSubItem::getType() const
{
    return mType;
}

void CLAbstractSubItem::mousePressEvent( QGraphicsSceneMouseEvent * event)
{
    event->accept();
}

void CLAbstractSubItem::mouseReleaseEvent( QGraphicsSceneMouseEvent * /*event*/)
{
    emit onPressed(this);
}
