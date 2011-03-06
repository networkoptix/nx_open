#include "unmoved_pixture_button.h"

#define OPACITY_TIME 500

extern QPixmap cached(const QString &img);

CLUnMovedPixture::CLUnMovedPixture(QString name, QGraphicsItem* parent, qreal normal_opacity, qreal active_opacity, QString img, int max_width, int max_height, qreal z):
CLUnMovedInteractiveOpacityItem(name, parent, normal_opacity, active_opacity)
{
	m_img = cached(img);

	setMaxSize(max_width, max_height);

	setZValue(z);

}

CLUnMovedPixture::~CLUnMovedPixture()
{

}

void CLUnMovedPixture::setMaxSize(int max_width, int max_height)
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

QSize CLUnMovedPixture::getSize() const
{
	return QSize(m_width, m_height);
}

QRectF CLUnMovedPixture::boundingRect() const
{
	return QRectF(0,0,m_width, m_height);
}

void CLUnMovedPixture::paint(QPainter *painter, const QStyleOptionGraphicsItem * /*option*/, QWidget * /*widget*/)
{
	painter->setRenderHint(QPainter::SmoothPixmapTransform);
	painter->setRenderHint(QPainter::Antialiasing);

	painter->drawPixmap(boundingRect(), m_img, m_img.rect());
}

//========================================================================================================
CLUnMovedPixtureButton::CLUnMovedPixtureButton(QString name, QGraphicsItem* parent, qreal normal_opacity, qreal active_opacity, QString img, int max_width, int max_height, qreal z):
CLUnMovedPixture(name, parent, normal_opacity, active_opacity, img, max_width, max_height,  z)
{

}

CLUnMovedPixtureButton::~CLUnMovedPixtureButton()
{

}

void CLUnMovedPixtureButton::mouseReleaseEvent(QGraphicsSceneMouseEvent* /*event*/)
{
	emit onPressed(m_name);
}

void CLUnMovedPixtureButton::mousePressEvent(QGraphicsSceneMouseEvent* /*event*/)
{
}
