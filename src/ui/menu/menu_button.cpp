#include "menu_button.h"
#include <QtGui>
#include <QFont>
#include <QMouseEvent>
#include "grapicsview_context_menu.h"

//=========================================================
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

//QColor buttonTextColor(QColor(255, 255, 255));
//QColor buttonTextColor(QColor(92, 113, 129));
//QColor buttonTextColor(QColor(0, 39, 198));
QColor buttonTextColor(QColor(255, 255, 255));
//=========================================================


TextButton::TextButton(const QString &text, QViewMenuHandler* handler):
mHandler(handler)
{
	this->text = text;
	this->state = NORMAL;
	setZValue(256*256); // always on top
	this->setAcceptsHoverEvents(true);
	setFlag(QGraphicsItem::ItemIgnoresTransformations, true);

	mWidth = 180*20;
	mHight = 19*20;

	setCursor(Qt::ArrowCursor);
}

TextButton::~TextButton()
{

}

void TextButton::setTopBottom(bool top, bool bottom)
{
	mTop = top;
	mBottom = bottom;
}

QRectF TextButton::boundingRect() const
{
	return QRectF(0,0,mWidth,mHight);
}

QString TextButton::getText() const
{
	return this->text;
}

void TextButton::setWidth(int width)
{
	mWidth = width;
}

void TextButton::setHeight(int height)
{
	mHight = height;
}


void TextButton::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *)
{
	//painter->fillRect(boundingRect(),QColor(255, 0, 0, 85));
	painter->setRenderHint(QPainter::SmoothPixmapTransform);
	painter->setRenderHint(QPainter::Antialiasing);


	

	unsigned char transp = 210;

	if (state == PRESSED)
	{
		
	} 
	if (state == HIGHLIGHT)
	{
		
		painter->fillRect(boundingRect(), QColor(100,0,0,transp));
		//painter->fillRect(boundingRect(), QColor(25,25,25,transp));
	}
	else if (state == DISABLED) 
	{

	}
	else if (state == NORMAL) 
	{
		painter->fillRect(boundingRect(), QColor(0,0,0,transp));
	}

	painter->setPen(QPen(QColor(100,100,100,230),  1, Qt::SolidLine));
	painter->drawLine(boundingRect().topLeft(), boundingRect().bottomLeft());
	painter->drawLine(boundingRect().topRight(), boundingRect().bottomRight());

	if (mTop)
		painter->drawLine(boundingRect().topLeft(), boundingRect().topRight());

	if (mBottom)
		painter->drawLine(boundingRect().bottomLeft(), boundingRect().bottomRight());


	//==================================================

	QGraphicsTextItem textItem(0, 0);
	textItem.setHtml(this->text);
	textItem.setTextWidth(boundingRect().width());
	textItem.setFont(buttonFont());
	textItem.setDefaultTextColor(buttonTextColor);
	textItem.document()->setDocumentMargin(2);

	float w = textItem.boundingRect().width();
	float h = textItem.boundingRect().height();
	QStyleOptionGraphicsItem style;
	textItem.paint(painter, &style, 0);


}

void TextButton::hoverEnterEvent(QGraphicsSceneHoverEvent *)
{
	if (this->state == DISABLED)
		return;

	if (this->state == NORMAL)
		this->setState(HIGHLIGHT);

}

void TextButton::hoverLeaveEvent(QGraphicsSceneHoverEvent *event)
{
	Q_UNUSED(event);
	if (this->state == DISABLED)
		return;

	this->setState(NORMAL);

}

void TextButton::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
	if (this->state == DISABLED)
		return;

	if (event->button() == Qt::LeftButton && this->state == HIGHLIGHT || this->state == NORMAL)
	{
		//this->setState(PRESSED);
		this->setState(NORMAL);
		//event->ignore();
		mHandler->OnMenuButton(0, getText());
	}
}

void TextButton::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{

}

void TextButton::mouseReleaseEvent(QMouseEvent *)
{
	if (this->state == PRESSED)
		this->setState(NORMAL);
}



void TextButton::setState(STATE state, bool upd)
{
	this->state = state;
	if (upd)
		update();
}

