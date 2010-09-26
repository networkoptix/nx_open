#ifndef static_image_item_h_0056
#define static_image_item_h_0056

#include "img_item.h"
#include <QPixmap>

class CLStaticImageItem : public CLImageItem
{
public:
	CLStaticImageItem (GraphicsView* view, int max_width, int max_height,
		QString img1filename, QString img2filename="",
		QString name="", QObject* handler=0);
protected:
	void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);
protected:
	QPixmap m_img1;
	QPixmap m_img2;

	bool m_img2_loaded;
	bool m_img1_loaded;

};

#endif//static_image_item_h_0056