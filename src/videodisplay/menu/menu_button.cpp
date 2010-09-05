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
	font.setFamily("Verdana");
#endif
	return font;
}

//QColor buttonTextColor(QColor(255, 255, 255));
//QColor buttonTextColor(QColor(92, 113, 129));
//QColor buttonTextColor(QColor(0, 39, 198));
QColor buttonTextColor(QColor(60, 99, 255));
//=========================================================


TextButton::TextButton(const QString &text, QObject* owner, QViewMenuHandler* handler):
mOwner(owner),
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

	painter->setPen(Qt::NoPen);


	QLinearGradient outlinebrush(0, 0, 0, boundingRect().height());
	QLinearGradient brush(0, 0, 0, boundingRect().height());

	brush.setSpread(QLinearGradient::PadSpread);
	qreal k = 4.0;
	//
	QColor highlight(255, 255, 255, 70*k);
	QColor shadow(0, 0, 0, 70*k);
	QColor sunken(220, 220, 220, 30*k);
	QColor normal1(255, 255, 245, 60*k);
	QColor normal2(255, 255, 235, 30*k);
	/**/

	/*/
	QColor highlight(255, 255, 255, 200);
	QColor shadow(0, 0, 0, 200);
	QColor sunken(220, 220, 220, 30*k);
	QColor normal1(255, 255, 245, 200);
	QColor normal2(255, 255, 235, 50);
	/**/

	//if (this->buttonType == TextButton::PANEL)
	{
		//normal1 = QColor(200, 170, 160, 50*k);
		//normal2 = QColor(50, 10, 0, 50*k);
	}

	if (state == PRESSED || state == DISABLED) 
	{
		outlinebrush.setColorAt(0.0f, shadow);
		outlinebrush.setColorAt(1.0f, highlight);
		brush.setColorAt(0.0f, sunken);
		painter->setPen(Qt::NoPen);
	} 
	else 
	{
		outlinebrush.setColorAt(1.0f, shadow);
		outlinebrush.setColorAt(0.0f, highlight);
		brush.setColorAt(0.0f, normal1);
		if (state!=HIGHLIGHT)
			brush.setColorAt(1.0f, normal2);
		painter->setPen(QPen(outlinebrush, 1));
	}
	painter->setBrush(brush);


	//if (this->buttonType== TextButton::PANEL)
		painter->drawRect(0, 0, boundingRect().width(), boundingRect().height());
	//else
		//painter->drawRoundedRect(0, 0, boundingRect().width(), boundingRect().height(), 10, 90, Qt::RelativeSize);

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
		mHandler->OnMenuButton(mOwner, getText());
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

