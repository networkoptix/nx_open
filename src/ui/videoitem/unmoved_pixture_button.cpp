#include "unmoved_pixture_button.h"
#include <QtGui>
#include <QPropertyAnimation>

#define OPACITY_TIME 500

extern QPixmap cached(const QString &img);

CLUnMovedPixture::CLUnMovedPixture(QGraphicsView* view, QString name, QString img, int max_width, int max_height, qreal z, qreal opacity):
CLAbstractUnmovedItem(view, name),
m_opacity(opacity)
{
	m_img = cached(img);
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


	setZValue(z);
	setOpacity(m_opacity);

}

CLUnMovedPixture::~CLUnMovedPixture()
{

}

QRectF CLUnMovedPixture::boundingRect() const
{
	return QRectF(0,0,m_width, m_height);
}

void CLUnMovedPixture::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
	painter->setRenderHint(QPainter::SmoothPixmapTransform);
	painter->setRenderHint(QPainter::Antialiasing);

	painter->drawPixmap(boundingRect(), m_img, m_img.rect());
}

//========================================================================================================


CLUnMovedPixtureButton::CLUnMovedPixtureButton(QGraphicsView* view, QString name, QString img, int max_width, int max_height, qreal z, qreal opacity):
CLUnMovedPixture(view, name, img, max_width, max_height, z, opacity),
m_animation(0)
{
	setAcceptsHoverEvents(true);
}

CLUnMovedPixtureButton::~CLUnMovedPixtureButton()
{
	stopAnimation();
}

void CLUnMovedPixtureButton::hoverEnterEvent(QGraphicsSceneHoverEvent *event)
{
	stopAnimation();

	m_animation = new QPropertyAnimation(this, "opacity");
	m_animation->setDuration(OPACITY_TIME);
	m_animation->setStartValue(opacity());
	m_animation->setEndValue(1.0);
	m_animation->start();	
	connect(m_animation, SIGNAL(finished ()), this, SLOT(stopAnimation()));
}

void CLUnMovedPixtureButton::hoverLeaveEvent(QGraphicsSceneHoverEvent *event)
{
	stopAnimation();

	m_animation = new QPropertyAnimation(this, "opacity");
	m_animation->setDuration(OPACITY_TIME);
	m_animation->setStartValue(opacity());
	m_animation->setEndValue(m_opacity);
	m_animation->start();	
	connect(m_animation, SIGNAL(finished ()), this, SLOT(stopAnimation()));

}

void CLUnMovedPixtureButton::mouseReleaseEvent( QGraphicsSceneMouseEvent * event )
{
	emit onPressed(m_name);
}

void CLUnMovedPixtureButton::mousePressEvent( QGraphicsSceneMouseEvent * event )
{

}

void CLUnMovedPixtureButton::stopAnimation()
{
	if (m_animation)
	{
		m_animation->stop();
		delete m_animation;
		m_animation = 0;
	}
}

//