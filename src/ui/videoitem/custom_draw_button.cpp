#include "custom_draw_button.h"

#include <QtGui>
#include <QFont>
#include <QFontmetrics>

QFont buttonFont()
{
	QFont font;
	font.setStyleStrategy(QFont::PreferAntialias);
#if 0//defined(Q_OS_MAC)
	font.setPixelSize(11);
	font.setFamily("Silom");
#else
	font.setPixelSize(15);
	font.setFamily("Bodoni MT");
#endif
	return font;
}

QColor buttonTextColor(QColor(255, 255, 255));

CLCustomBtnItem::CLCustomBtnItem(GraphicsView* view, int max_width, int max_height, 
								 QString name, QObject* handler,
								 QString text, QString tooltipText):
CLAbstractSceneItem(view,max_width,max_height, name, handler),
mText(text)
{
	setToolTip(tooltipText);
}

CLCustomBtnItem::~CLCustomBtnItem()
{

}


void CLCustomBtnItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
	//painter->fillRect(boundingRect(),QColor(255, 0, 0, 85));
	//painter->setRenderHint(QPainter::SmoothPixmapTransform);
	//painter->setRenderHint(QPainter::Antialiasing);


	unsigned char transp = 210;

	if (m_mouse_over)
	{

		painter->fillRect(boundingRect(), QColor(0,0,255,transp));
		//painter->fillRect(boundingRect(), QColor(25,25,25,transp));
	}
	else
	{
		painter->fillRect(boundingRect(), QColor(0,0,155,transp));
	}

	painter->setPen(QPen(QColor(100,100,100,230),  1, Qt::SolidLine));
	painter->drawRect(boundingRect());
	
	//==================================================
	QFont  m_FPS_Font("Courier New", 350);
	painter->setFont(m_FPS_Font);

	QFontMetrics metrics = QFontMetrics(m_FPS_Font);
	int border = qMax(4, metrics.leading());

	QRect rect(0, 0, width() , height());
	painter->setPen(QColor(255, 255, 255, 110));

	painter->drawText((width() - rect.width())/2, border,
		rect.width(), rect.height(),
		Qt::AlignCenter | Qt::TextWordWrap, mText);
}

