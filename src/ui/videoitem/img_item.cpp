#include "img_item.h"
#include <QFont>
#include <QPainter>
#include <QPaintEngine>
#include <QTextStream>



CLImageItem::CLImageItem(GraphicsView* view, int max_width, int max_height,
						 QString name, QObject* handler):
CLAbstractSceneItem(view, max_width, max_height,name,handler),
m_imageWidth(m_max_width),
m_imageHeight(m_max_height),
m_imageWidth_old(m_max_width),
m_imageHeight_old(m_max_height),
m_aspectratio((qreal)m_imageWidth/m_imageHeight),
m_showinfotext(false),
m_showimagesize(false),
m_showing_text(false),
m_timeStarted(false),
m_Info_Font("times", 110)
{
	m_type = IMAGE;
	m_Info_Font.setWeight(QFont::Bold);
}

int CLImageItem::height() const
{
	return width()/aspectRatio();
}


int CLImageItem::width() const
{
	qreal ar = aspectRatio();
	qreal ar_f = (qreal)m_max_width/m_max_height;

	if (ar>ar_f) // width > hight; normal scenario; ar = w/h
	{
		return m_max_width;
	}
	else 
		return m_max_height*aspectRatio();
}


float CLImageItem::aspectRatio() const
{
	QMutexLocker  locker(&m_mutex_aspect);	
	return m_aspectratio;
}

int CLImageItem::imageWidth() const
{
	QMutexLocker locker(&m_mutex);
	return m_imageWidth;

}

int CLImageItem::imageHeight() const
{
	QMutexLocker locker(&m_mutex);
	return m_imageHeight;
}

void CLImageItem::setInfoText(QString text)
{
	m_infoText = text;
}

void CLImageItem::setShowInfoText(bool show)
{
	m_showinfotext = show;
}

void CLImageItem::setShowImagesize(bool show)
{
	m_showimagesize = show;
}

bool CLImageItem::wantText() const
{
	return (m_showinfotext || m_showimagesize);
}

void CLImageItem::drawStuff(QPainter* painter)
{
	if (!wantText())
	{
		m_showing_text = false;
		m_timeStarted = false;
	}

	if (!m_showing_text && wantText())
	{
		if (m_timeStarted)
		{
			int ms = m_textTime.msecsTo(QTime::currentTime());
			if (ms>350)
				m_showing_text = true;

		}
		else
		{
			m_textTime.restart();
			m_timeStarted = true;
		}
	}
	//===================================


	if (m_showing_text)
	{
		if (m_showinfotext || m_showimagesize)
		{
			drawInfoText(painter); // ahtung! drawText takes huge(!) ammount of cpu
		}
	}

	if (m_draw_rotation_helper)
		drawRotationHelper(painter);


}

void CLImageItem::drawInfoText(QPainter* painter)
{
	painter->setFont(m_Info_Font);
	//painter->setPen(QColor(255,0,0,220));
	painter->setPen(QColor(30,55,255));



	QFontMetrics fm(painter->font());

	QString text;
	QTextStream str(&text);

	if (m_showimagesize)
		str << "(" << imageWidth() << "x" << imageHeight() <<") ";

	if (m_showinfotext)
		str << m_infoText;


	painter->drawText(2,20 + fm.height()/2, text);
	//painter->drawText(2,20 + 100, text);

}

