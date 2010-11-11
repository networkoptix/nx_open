#include "abstract_image_sub_item.h"
#include <QPainter>

CLAbstractImgSubItem::CLAbstractImgSubItem(CLAbstractSceneItem* parent, qreal opacity, int max_width, int max_height):
CLAbstractSubItem(parent, opacity)
{

}

CLAbstractImgSubItem::~CLAbstractImgSubItem()
{

}


void CLAbstractImgSubItem::setMaxSize(int max_width, int max_height)
{
	m_width = m_img.width();
	m_height = m_img.height();

	qreal aspect = qreal(m_width)/m_height;

	qreal ar_f = (qreal)max_width/max_height;

	if (aspect>ar_f) // width > hight; normal scenario; ar = w/h
	{
		m_width = max_width;
		m_height = m_width/aspect;
	}
	else
	{
		m_width = max_height*aspect;
		m_height = max_height;
	}
}


QRectF CLAbstractImgSubItem::boundingRect() const
{
	return QRectF(0,0,m_width, m_height);
}

void CLAbstractImgSubItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
	painter->setRenderHint(QPainter::SmoothPixmapTransform);
	painter->setRenderHint(QPainter::Antialiasing);

	painter->drawPixmap(boundingRect(), m_img, m_img.rect());
}
