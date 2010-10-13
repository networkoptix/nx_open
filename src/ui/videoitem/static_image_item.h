#ifndef static_image_item_h_0056
#define static_image_item_h_0056

#include "img_item.h"
#include <QPixmap>

class CLStaticImageItem : public CLImageItem
{
public:
	CLStaticImageItem (GraphicsView* view, int max_width, int max_height,
		QString imgfilename, QString name="");
protected:
	void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);
protected:
	QPixmap m_img;

};

#endif//static_image_item_h_0056