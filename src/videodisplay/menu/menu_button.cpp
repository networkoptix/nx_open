#include "menu_button.h"
#include <QtGui>
#include <QFont>

#define BUTTON_WIDTH 180*20
#define BUTTON_HEIGHT 19*20

//=========================================================
QFont buttonFont()
{
	QFont font;
	font.setStyleStrategy(QFont::PreferAntialias);
#if 0//defined(Q_OS_MAC)
	font.setPixelSize(11);
	font.setFamily("Silom");
#else
	font.setPixelSize(11);
	font.setFamily("Verdana");
#endif
	return font;
}

QColor buttonText(QColor(255, 255, 255));
//=========================================================


TextButton::TextButton(const QString &text)		   
{
	this->text = text;
	this->state = NORMAL;
	setZValue(256*256); // always on top
	this->setAcceptsHoverEvents(true);
	//setFlag(QGraphicsItem::ItemIgnoresTransformations, true);

}

TextButton::~TextButton()
{

}

QRectF TextButton::boundingRect() const
{
	return QRectF(0,0,BUTTON_WIDTH,BUTTON_HEIGHT);
}

QString TextButton::getText() const
{
	return this->text;
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
	QColor highlight(255, 255, 255, 70);
	QColor shadow(0, 0, 0, 70);
	QColor sunken(220, 220, 220, 30);
	QColor normal1(255, 255, 245, 60);
	QColor normal2(255, 255, 235, 10);

	//if (this->buttonType == TextButton::PANEL)
	{
		normal1 = QColor(200, 170, 160, 50);
		normal2 = QColor(50, 10, 0, 50);
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

void TextButton::mousePressEvent(QGraphicsSceneMouseEvent *)
{
	if (this->state == DISABLED)
		return;

	if (this->state == HIGHLIGHT || this->state == NORMAL)
		this->setState(PRESSED);
}

void TextButton::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
	if (this->state == PRESSED)
		this->setState(NORMAL);
}



void TextButton::setState(STATE state)
{
	this->state = state;
	update();
}

