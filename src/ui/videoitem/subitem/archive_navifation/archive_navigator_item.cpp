#include "archive_navigator_item.h"
#include "..\..\abstract_scene_item.h"
#include <QPainter>

int NavigatorItemHeight = 200;

CLArchiveNavigatorItem::CLArchiveNavigatorItem(CLAbstractSceneItem* parent):
CLAbstractSubItem(parent, 0.2, 0.8)
{
	m_height = NavigatorItemHeight;
	m_width = parent->boundingRect().width();
	mType = ArchiveNavigator;
}

CLArchiveNavigatorItem::~CLArchiveNavigatorItem()
{

}

// this function uses parent width
void CLArchiveNavigatorItem::onResize()
{
	m_width = parentItem()->boundingRect().width();
}


void CLArchiveNavigatorItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
	painter->fillRect(boundingRect(),QColor(0, 0, 0));
}

QRectF CLArchiveNavigatorItem::boundingRect() const
{
	return QRectF(0,0,m_width, m_height);
}
