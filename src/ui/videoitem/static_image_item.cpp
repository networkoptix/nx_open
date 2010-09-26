#include "static_image_item.h"

#include <QtGui>

CLStaticImageItem::CLStaticImageItem(GraphicsView* view, int max_width, int max_height,
				   QString img1filename, QString img2filename,
				   QString name, QObject* handler):
CLImageItem(view, max_width,max_height,name,handler)
{
	if (img1filename!="")
	{
		if (m_img1.load(img1filename))
		{
				m_imageWidth_old = m_imageWidth = m_img1.width();
				m_imageHeight_old = m_imageHeight = m_img1.height();

				m_aspectratio = qreal(m_imageWidth)/m_imageHeight;

		}
	}

	if (img2filename!="")
		m_img1.load(img2filename);


}

void CLStaticImageItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{

	if (m_img2_loaded && m_mouse_over)
		painter->drawPixmap(boundingRect(), m_img2, m_img2.rect());
	else
		painter->drawPixmap(boundingRect(), m_img1, m_img1.rect());

	drawStuff(painter);
}


//