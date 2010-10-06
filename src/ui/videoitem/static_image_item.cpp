#include "static_image_item.h"

#include <QtGui>

extern QPixmap cached(const QString &img);

CLStaticImageItem::CLStaticImageItem(GraphicsView* view, int max_width, int max_height,
				   QString imgfilename, 
				   QString name, QObject* handler):
CLImageItem(view, max_width,max_height,name,handler)
{

	m_img = cached(imgfilename);

	m_imageWidth_old = m_imageWidth = m_img.width();
	m_imageHeight_old = m_imageHeight = m_img.height();

	m_aspectratio = qreal(m_imageWidth)/m_imageHeight;

	//m_img.setMask(m_img.createHeuristicMask());

}

void CLStaticImageItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
	painter->setRenderHint(QPainter::SmoothPixmapTransform);
	painter->setRenderHint(QPainter::Antialiasing);

	painter->drawPixmap(boundingRect(), m_img, m_img.rect());
	drawStuff(painter);
}


//