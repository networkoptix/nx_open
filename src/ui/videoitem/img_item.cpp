#include "img_item.h"
#include <QFont>
#include <QPainter>
#include <QPaintEngine>
#include <QTextStream>
#include <math.h>

static const double Pi = 3.14159265358979323846264338327950288419717;
static double TwoPi = 2.0 * Pi;





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
m_draw_rotation_helper(false),
m_Info_Font("times", 110)
{
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


	drawShadow(painter);


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

void CLImageItem::drawRotationHelper(bool val)
{
	m_draw_rotation_helper = val;
}

void CLImageItem::setRotationPoint(QPointF point1, QPointF point2)
{
	m_rotation_center = point1;
	m_rotation_hand = point2;
}

void CLImageItem::drawRotationHelper(QPainter* painter)
{
	static QColor color1(255, 0, 0, 250);
	static QColor color2(255, 0, 0, 100);

	static const int r = 30;
	static const int penWidth = 6;
	static const int p_line_len = 220;
	static const int arrowSize = 30;



	QRadialGradient gradient(m_rotation_center, r);
	gradient.setColorAt(0, color1);
	gradient.setColorAt(1, color2);

	painter->save();

	painter->setPen(QPen(color2, 0, Qt::SolidLine));



	painter->setBrush(gradient);
	painter->drawEllipse(m_rotation_center, r, r);

	painter->setPen(QPen(color2, penWidth, Qt::SolidLine));
	painter->drawLine(m_rotation_center,m_rotation_hand);

	// building new line
	QLineF line(m_rotation_hand, m_rotation_center);
	QLineF line_p = line.unitVector().normalVector();
	line_p.setLength(p_line_len/2);

	line_p = QLineF(line_p.p2(),line_p.p1());
	line_p.setLength(p_line_len);



	painter->drawLine(line_p);




	double angle = ::acos(line_p.dx() / line_p.length());
	if (line_p.dy() >= 0)
		angle = TwoPi - angle;

	qreal s = 2.5;

	QPointF sourceArrowP1 = line_p.p1() + QPointF(sin(angle + Pi / s) * arrowSize,
		cos(angle + Pi / s) * arrowSize);
	QPointF sourceArrowP2 = line_p.p1() + QPointF(sin(angle + Pi - Pi / s) * arrowSize,
		cos(angle + Pi - Pi / s) * arrowSize);   
	QPointF destArrowP1 = line_p.p2() + QPointF(sin(angle - Pi / s) * arrowSize,
		cos(angle - Pi / s) * arrowSize);
	QPointF destArrowP2 = line_p.p2() + QPointF(sin(angle - Pi + Pi / s) * arrowSize,
		cos(angle - Pi + Pi / s) * arrowSize);

	painter->setBrush(color2);
	painter->drawPolygon(QPolygonF() << line_p.p1() << sourceArrowP1 << sourceArrowP2);
	painter->drawPolygon(QPolygonF() << line_p.p2() << destArrowP1 << destArrowP2);        

	painter->restore();
	/**/

}