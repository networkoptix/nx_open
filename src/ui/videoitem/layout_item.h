#ifndef layout_item_h1421_h
#define layout_item_h1421_h


#include "img_item.h"

class LayoutContent;

class CLLayoutItem : public CLImageItem
{
	Q_OBJECT
public:
	CLLayoutItem (GraphicsView* view, int max_width, int max_height, QString name, QString tooltip);
	~CLLayoutItem ();

	void setRefContent(LayoutContent* cont);
	LayoutContent* getRefContent() const;

protected:
	QPointF getBestSubItemPos(CLSubItemType type);
	void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = 0);
protected:

	LayoutContent* mContent; //this item references to this content

    QPixmap mPixmap;

};

#endif //layout_item_h1421_h
