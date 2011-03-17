#ifndef dynamic_image_item_h_2103
#define dynamic_image_item_h_2103

#include "img_item.h"

class CLPictureImageItem : public CLImageItem
{
    Q_OBJECT
public:
	CLPictureImageItem(GraphicsView* view, int max_width, int max_height,
		QString path, QString name="");
    ~CLPictureImageItem();
protected:
	void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);

protected slots:
    void onTimer();

protected:
	const QPixmap *m_CurrentImg;
    QList<QPixmap> m_Images;
    QTimer m_Timer;

    int m_CurrentIndex;
    bool m_forwardDirection;

};

#endif//static_image_item_h_0056