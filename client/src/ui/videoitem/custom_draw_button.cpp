#include "custom_draw_button.h"

#include "settings.h"

QFont buttonFont()
{
	QFont font;
	font.setStyleStrategy(QFont::PreferAntialias);
#if 0//defined(Q_OS_MAC)
	font.setPixelSize(11);
	font.setFamily(QLatin1String("Silom"));
#else
	font.setPixelSize(15);
	font.setFamily(QLatin1String("Bodoni MT"));
#endif
	return font;
}

QColor buttonTextColor(QColor(255, 255, 255));

CLCustomBtnItem::CLCustomBtnItem(GraphicsView* view, int max_width, int max_height,
								 QString name, QString text, QString tooltipText)
	: CLAbstractSceneItem(view, max_width, max_height, name),
	  mText(text)
{
	//setFocusPolicy(Qt::NoFocus);
	setToolTip(tooltipText);
}

CLCustomBtnItem::~CLCustomBtnItem()
{
}

void CLCustomBtnItem::createPaths(int radius)
{
	mSmallRectPath = createRoundRectpath(width()*0.94,height()*0.94, radius);
	mSmallRectPath.translate(width()*0.03, height()*0.03);

	mRoundRectPath = createRoundRectpath(width(),height(), radius);
	mShadowRectPath = mRoundRectPath.translated(100,100);
}

QPainterPath CLCustomBtnItem::createRoundRectpath(int width, int height, int radius)
{
	QPainterPath result;

	result.moveTo(width, radius);
	result.arcTo(width - radius, 0, radius, radius, 0.0, 90.0);
	result.lineTo(radius, 0);
	result.arcTo(0, 0, radius, radius, 90.0, 90.0);
	result.lineTo(0, height-radius);
	result.arcTo(0, height - radius, radius, radius, 180.0, 90.0);
	result.lineTo(width-radius, height);
	result.arcTo(width-radius, height -  radius, radius, radius, 270.0, 90.0);
	result.closeSubpath();

	return result;
}

void CLCustomBtnItem::paint(QPainter *painter, const QStyleOptionGraphicsItem * /*option*/, QWidget * /*widget*/)
{
	//painter->fillRect(boundingRect(),QColor(255, 0, 0, 85));
	//painter->setRenderHint(QPainter::SmoothPixmapTransform);
	//painter->setRenderHint(QPainter::Antialiasing);

	painter->fillPath(mShadowRectPath, global_shadow_color);

	QColor border_color(20,20,160);
	if (m_mouse_over)
		border_color = QColor(20,20,190);

	painter->fillPath(mRoundRectPath, border_color );

	QColor btn_color(25,25,200);
	if (m_mouse_over)
		btn_color = QColor(25,25,230);

	painter->fillPath(mSmallRectPath, btn_color);

	//painter->setPen(QPen(QColor(100,100,100,230),  1, Qt::SolidLine));
	//painter->drawRect(boundingRect());

	//==================================================
	QFont font(QLatin1String("Courier New"), 350);
	font.setWeight(QFont::Bold);
	painter->setFont(font);

	QFontMetrics metrics = QFontMetrics(font);
	int border = qMax(4, metrics.leading());

	QRect rect(0, 0, width() , height());

	if (m_mouse_over)
		painter->setPen(QColor(190, 190, 190));
	else
		painter->setPen(QColor(150, 150, 150));

	painter->drawText((width() - rect.width())/2, border,
		rect.width(), rect.height(),
		Qt::AlignCenter | Qt::TextWordWrap, mText);
}
