#include "custom_draw_button.h"

#include <QtGui>
#include <QFont>

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
CLAbstractSceneItem(view,m_max_width,max_height, name, handler),
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

		painter->fillRect(boundingRect(), QColor(100,0,0,transp));
		//painter->fillRect(boundingRect(), QColor(25,25,25,transp));
	}
	else
	{
		painter->fillRect(boundingRect(), QColor(0,0,0,transp));
	}

	painter->setPen(QPen(QColor(100,100,100,230),  1, Qt::SolidLine));
	painter->drawRect(boundingRect());
	
	//==================================================

	QGraphicsTextItem textItem(0, 0);
	textItem.setHtml(mText);
	textItem.setTextWidth(boundingRect().width());
	textItem.setFont(buttonFont());
	textItem.setDefaultTextColor(buttonTextColor);
	textItem.document()->setDocumentMargin(2);

	float w = textItem.boundingRect().width();
	float h = textItem.boundingRect().height();
	QStyleOptionGraphicsItem style;
	textItem.paint(painter, &style, 0);
}

