#include "menu_button.h"
#include <QtGui>
#include <QFont>

#define BUTTON_WIDTH 180
#define BUTTON_HEIGHT 19

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


class ButtonBackground : public QGraphicsItem
{
public:
	TextButton::BUTTONTYPE type;
	bool highlighted;
	bool pressed;
	QSize logicalSize;

	ButtonBackground(TextButton::BUTTONTYPE type, bool highlighted, bool pressed, QSize logicalSize,
		QGraphicsScene *scene, QGraphicsItem *parent) : QGraphicsItem(parent, scene)
	{
		this->type = type;
		this->highlighted = highlighted;
		this->pressed = pressed;
		this->logicalSize = logicalSize;
		
		mImage = createImage(QMatrix());
	}

	void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = 0)
	{
		bool wasSmoothPixmapTransform = painter->testRenderHint(QPainter::SmoothPixmapTransform);
		painter->setRenderHint(QPainter::SmoothPixmapTransform);


		QMatrix m = painter->worldMatrix();
		painter->setWorldMatrix(QMatrix());
		float x = m.dx();
		float y = m.dy();

		painter->drawImage(QPointF(x, y), *mImage);


	}

	~ButtonBackground()
	{
		delete mImage;
	}

protected:
	QImage * mImage;
protected:
	QImage *createImage(const QMatrix &matrix) const
	{
		if (type == TextButton::SIDEBAR || type == TextButton::PANEL)
			return createRoundButtonBackground(matrix);
		else
			return createArrowBackground(matrix);
	}

	QImage *createRoundButtonBackground(const QMatrix &matrix) const
	{
		QRect scaledRect;
		scaledRect = matrix.mapRect(QRect(0, 0, this->logicalSize.width(), this->logicalSize.height()));

		QImage *image = new QImage(scaledRect.width(), scaledRect.height(), QImage::Format_ARGB32_Premultiplied);
		image->fill(QColor(0, 0, 0, 0).rgba());
		QPainter painter(image);
		painter.setRenderHint(QPainter::SmoothPixmapTransform);
		painter.setRenderHint(QPainter::Antialiasing);
		painter.setPen(Qt::NoPen);

		QLinearGradient outlinebrush(0, 0, 0, scaledRect.height());
		QLinearGradient brush(0, 0, 0, scaledRect.height());

		brush.setSpread(QLinearGradient::PadSpread);
		QColor highlight(255, 255, 255, 70);
		QColor shadow(0, 0, 0, 70);
		QColor sunken(220, 220, 220, 30);
		QColor normal1(255, 255, 245, 60);
		QColor normal2(255, 255, 235, 10);

		if (this->type == TextButton::PANEL){
			normal1 = QColor(200, 170, 160, 50);
			normal2 = QColor(50, 10, 0, 50);
		}

		if (pressed) {
			outlinebrush.setColorAt(0.0f, shadow);
			outlinebrush.setColorAt(1.0f, highlight);
			brush.setColorAt(0.0f, sunken);
			painter.setPen(Qt::NoPen);
		} else {
			outlinebrush.setColorAt(1.0f, shadow);
			outlinebrush.setColorAt(0.0f, highlight);
			brush.setColorAt(0.0f, normal1);
			if (!this->highlighted)
				brush.setColorAt(1.0f, normal2);
			painter.setPen(QPen(outlinebrush, 1));
		}
		painter.setBrush(brush);


		if (this->type == TextButton::PANEL)
			painter.drawRect(0, 0, scaledRect.width(), scaledRect.height());
		else
			painter.drawRoundedRect(0, 0, scaledRect.width(), scaledRect.height(), 10, 90, Qt::RelativeSize);
		return image;
	}

	QImage *createArrowBackground(const QMatrix &matrix) const
	{
		QRect scaledRect;
		scaledRect = matrix.mapRect(QRect(0, 0, this->logicalSize.width(), this->logicalSize.height()));

		QImage *image = new QImage(scaledRect.width(), scaledRect.height(), QImage::Format_ARGB32_Premultiplied);
		image->fill(QColor(0, 0, 0, 0).rgba());
		QPainter painter(image);
		painter.setRenderHint(QPainter::SmoothPixmapTransform);
		painter.setRenderHint(QPainter::Antialiasing);
		painter.setPen(Qt::NoPen);

		QLinearGradient outlinebrush(0, 0, 0, scaledRect.height());
		QLinearGradient brush(0, 0, 0, scaledRect.height());

		brush.setSpread(QLinearGradient::PadSpread);
		QColor highlight(255, 255, 255, 70);
		QColor shadow(0, 0, 0, 70);
		QColor sunken(220, 220, 220, 30);
		QColor normal1 = QColor(200, 170, 160, 50);
		QColor normal2 = QColor(50, 10, 0, 50);

		if (pressed) {
			outlinebrush.setColorAt(0.0f, shadow);
			outlinebrush.setColorAt(1.0f, highlight);
			brush.setColorAt(0.0f, sunken);
			painter.setPen(Qt::NoPen);
		} else {
			outlinebrush.setColorAt(1.0f, shadow);
			outlinebrush.setColorAt(0.0f, highlight);
			brush.setColorAt(0.0f, normal1);
			if (!this->highlighted)
				brush.setColorAt(1.0f, normal2);
			painter.setPen(QPen(outlinebrush, 1));
		}
		painter.setBrush(brush);


		painter.drawRect(0, 0, scaledRect.width(), scaledRect.height());

		float xOff = scaledRect.width() / 2;
		float yOff = scaledRect.height() / 2;
		float sizex = 3.0f * matrix.m11();
		float sizey = 1.5f * matrix.m22();
		if (this->type == TextButton::UP)
			sizey *= -1;
		QPainterPath path;
		path.moveTo(xOff, yOff + (5 * sizey));
		path.lineTo(xOff - (4 * sizex), yOff - (3 * sizey));
		path.lineTo(xOff + (4 * sizex), yOff - (3 * sizey));
		path.lineTo(xOff, yOff + (5 * sizey));
		painter.drawPath(path);

		return image;
	}

	QRectF boundingRect() const
	{
		return QRectF(0, 0, BUTTON_WIDTH, BUTTON_HEIGHT); // Sorry for using magic number
	}

};


//===============================================================================================

class TextItem : public QGraphicsItem
{
public:
	enum TYPE {STATIC_TEXT, DYNAMIC_TEXT};

	TextItem(const QString &text, const QFont &font, const QColor &textColor,
		float textWidth, QGraphicsScene *scene = 0, QGraphicsItem *parent = 0, TYPE type = STATIC_TEXT, const QColor &bgColor = QColor());

	~TextItem();

	void setText(const QString &text);
	QRectF boundingRect() const; // overridden
	void animationStarted(int id = 0);
	void animationStopped(int id = 0);

protected:
	virtual QImage *createImage(const QMatrix &matrix) const; // overridden
	virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option = 0, QWidget *widget = 0); // overridden

private:
	float textWidth;
	QString text;
	QFont font;
	QColor textColor;
	QColor bgColor;
	TYPE type;
	QImage* img;
};


TextItem::TextItem(const QString &text, const QFont &font, const QColor &textColor,
						   float textWidth, QGraphicsScene *scene, QGraphicsItem *parent, TYPE type, const QColor &bgColor)
						   : QGraphicsItem(parent, scene)
{
	this->type = type;
	this->text = text;
	this->font = font;
	this->textColor = textColor;
	this->bgColor = bgColor;
	this->textWidth = textWidth;
	img = createImage(QMatrix());
}

TextItem::~TextItem()
{
	delete img;
}

void TextItem::setText(const QString &text)
{
	this->text = text;
	this->update();
}

QImage *TextItem::createImage(const QMatrix &matrix) const
{
	if (this->type == DYNAMIC_TEXT)
		return 0;

	float sx = qMin(matrix.m11(), matrix.m22());
	float sy = matrix.m22() < sx ? sx : matrix.m22();

	QGraphicsTextItem textItem(0, 0);
	textItem.setHtml(this->text);
	textItem.setTextWidth(this->textWidth);
	textItem.setFont(this->font);
	textItem.setDefaultTextColor(this->textColor);
	textItem.document()->setDocumentMargin(2);

	float w = textItem.boundingRect().width();
	float h = textItem.boundingRect().height();
	QImage *image = new QImage(int(w * sx), int(h * sy), QImage::Format_ARGB32_Premultiplied);
	image->fill(QColor(0, 0, 0, 0).rgba());
	QPainter painter(image);
	painter.scale(sx, sy);
	QStyleOptionGraphicsItem style;
	textItem.paint(&painter, &style, 0);
	return image;
}



QRectF TextItem::boundingRect() const

{
	return QRectF(0, 0, BUTTON_WIDTH, BUTTON_HEIGHT); // Sorry for using magic number
}


void TextItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
	Q_UNUSED(option);
	Q_UNUSED(widget);

	if (this->type == STATIC_TEXT) 
	{
		bool wasSmoothPixmapTransform = painter->testRenderHint(QPainter::SmoothPixmapTransform);
		painter->setRenderHint(QPainter::SmoothPixmapTransform);


		QMatrix m = painter->worldMatrix();
		painter->setWorldMatrix(QMatrix());
		float x = m.dx();
		float y = m.dy();

		painter->drawImage(QPointF(x, y), *img);
	}

	painter->setPen(this->textColor);
	painter->drawText(0, 0, this->text);
}

//=======================================================================================================

TextButton::TextButton(const QString &text, ALIGNMENT align, int userCode,
					   QGraphicsScene *scene, QGraphicsItem *parent, BUTTONTYPE type)
					   : QGraphicsItem(parent, scene)
{
	this->menuString = text;
	this->buttonLabel = text;
	this->alignment = align;
	this->buttonType = type;
	this->userCode = userCode;
	this->bgOn = 0;
	this->bgOff = 0;
	this->bgHighlight = 0;
	this->bgDisabled = 0;
	this->state = OFF;
	//setZValue(1.0); // always on top

	this->setAcceptsHoverEvents(true);
	this->setCursor(Qt::PointingHandCursor);

	// Calculate button size:
	const int w = BUTTON_WIDTH;
	const int h = BUTTON_HEIGHT;
	if (type == SIDEBAR || type == PANEL)
		this->logicalSize = QSize(w, h);
	else
		this->logicalSize = QSize(int((w / 2.0f) - 5), int(h * 1.5f));

	prepare();
}

QString TextButton::getText() const
{
	return this->buttonLabel;
}

void TextButton::setMenuString(const QString &menu)
{
	this->menuString = menu;
}

void TextButton::prepare()
{
	this->setupHoverText();
	this->setupButtonBg();
}

TextButton::~TextButton()
{

}

QRectF TextButton::boundingRect() const
{
	return QRectF(0, 0, this->logicalSize.width(), this->logicalSize.height());
};

void TextButton::setupHoverText()
{
	if (this->buttonLabel.isEmpty())
		return;

	TextItem *textItem = new TextItem(this->buttonLabel, buttonFont(), buttonText, -1, this->scene(), this);
	textItem->setZValue(zValue() + 2);
	textItem->setPos(16, 0);
}


void TextButton::setState(STATE state)
{
	this->state = state;
	this->bgOn->setVisible(state == ON);
	this->bgOff->setVisible(state == OFF);
	this->bgHighlight->setVisible(state == HIGHLIGHT);
	this->bgDisabled->setVisible(state == DISABLED);
	this->setCursor(state == DISABLED ? Qt::ArrowCursor : Qt::PointingHandCursor);

}

void TextButton::setupButtonBg()
{
	this->bgOn = new ButtonBackground(this->buttonType, true, true, this->logicalSize, this->scene(), this);
	this->bgOff = new ButtonBackground(this->buttonType, false, false, this->logicalSize, this->scene(), this);
	this->bgHighlight = new ButtonBackground(this->buttonType, true, false, this->logicalSize, this->scene(), this);
	this->bgDisabled = new ButtonBackground(this->buttonType, true, true, this->logicalSize, this->scene(), this);
	this->setState(OFF);
}

void TextButton::hoverEnterEvent(QGraphicsSceneHoverEvent *)
{
	if (this->state == DISABLED)
		return;

	if (this->state == OFF)
		this->setState(HIGHLIGHT);

}

void TextButton::hoverLeaveEvent(QGraphicsSceneHoverEvent *)
{
	//Q_UNUSED(event);

	if (this->state == DISABLED)
		return;

	this->setState(OFF);

}

void TextButton::mousePressEvent(QGraphicsSceneMouseEvent *)
{
	if (this->state == DISABLED)
		return;

	if (this->state == HIGHLIGHT || this->state == OFF)
		this->setState(ON);
}

void TextButton::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
	if (this->state == ON){
		this->setState(OFF);
		if (this->boundingRect().contains(event->pos()))
		{
			//MenuManager::instance()->itemSelected(this->userCode, this->menuString);
			return;
		}
	}
}



