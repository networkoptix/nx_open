#ifndef archive_navigator_item_h1756
#define archive_navigator_item_h1756

#include "..\abstract_sub_item.h"



class CLArchiveNavigatorItem : public CLAbstractSubItem
{
	Q_OBJECT
public:
	CLArchiveNavigatorItem(CLAbstractSceneItem* parent);
	~CLArchiveNavigatorItem();

	// this function uses parent width
	void onResize();

protected:
	void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);
	virtual QRectF boundingRect() const;
protected:
	int m_width;
	int m_height;

};

#endif  //archive_navigator_item_h1756