#include "close_sub_item.h"

extern QPixmap cached(const QString &img);

CLCloseSubItem::CLCloseSubItem(CLAbstractSceneItem* parent, qreal opacity, int max_width, int max_height):
CLAbstractImgSubItem(parent, opacity, max_width, max_height)
{
	m_img = cached("./skin/close2.png");
	setMaxSize(max_width, max_height);
}

CLCloseSubItem::~CLCloseSubItem()
{

}


CLAbstractSubItem::ItemType CLCloseSubItem::getType() const
{
	return Close;
}