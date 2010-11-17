#ifndef abstract_img_sub_item_h1440
#define abstract_img_sub_item_h1440

#include "abstract_sub_item.h"

class CLImgSubItem : public CLAbstractSubItem
{
public:
	CLImgSubItem(CLAbstractSceneItem* parent, const QString& img, CLAbstractSubItem::ItemType type, 
		qreal opacity, qreal max_opacity, int max_width, int max_height);
	~CLImgSubItem();

	void onResize();
protected:
	void setMaxSize(int max_width, int max_height);
	QRectF boundingRect() const;
	void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);

protected:
	QPixmap m_img;
	int m_width;
	int m_height;
};


#endif //abstract_img_sub_item_h1440