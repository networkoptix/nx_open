#include "static_image_widget.h"

extern QPixmap cached(const QString &img);

CLStaticImageWidget::CLStaticImageWidget(QWidget *parent, QString img, QString name, int max_width):
QGLWidget(parent,0)//, Qt::Window)
{
	hide();
	m_img = cached(img);

	int w = m_img.width();
	int h = m_img.height();

	qreal apect = (qreal)w/h;
	
	m_width = max_width;
	m_height = max_width/apect;

	QSize size(m_width, m_height);

	setMaximumSize(size);
	setMinimumSize(size);

	setWindowOpacity(0.5);

	

}

void CLStaticImageWidget::paintEvent(QPaintEvent *event)
{

	/*
	// Create new picture for transparent
	QPixmap transparent(m_img.size());
	// Do transparency
	//transparent.fill(Qt::transparent);
	QPainter p;
	p.begin(&transparent);
	p.setCompositionMode(QPainter::CompositionMode_Source);

	p.drawPixmap(0, 0, m_img);

	p.setCompositionMode(QPainter::CompositionMode_DestinationIn);

	p.fillRect(transparent.rect(), QColor(0, 0, 0, 150));
	p.end();



	QPainter painter(this);
	painter.drawPixmap(QRect(0,0,m_width,m_height), transparent, transparent.rect());
	painter.end();
	/**/

	QPainter painter(this);
	//painter.drawPixmap(QRect(0,0,m_width,m_height), m_img, m_img.rect());
	painter.fillRect(QRect(0,0,m_width,m_height), QColor(255, 0,0, 20));
	painter.end();

}

void CLStaticImageWidget::mouseReleaseEvent ( QMouseEvent * event )
{

}


