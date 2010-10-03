#include "custom_draw_button.h"

#include <QtGui>
#include <QFont>
#include <QFontmetrics>
#include "settings.h"

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

	//setFocusPolicy(Qt::NoFocus);

	setToolTip(tooltipText);

	int radius = width()/4;
	mRoundRectPath.moveTo(width(), radius);
	mRoundRectPath.arcTo(width() - radius, 0, radius, radius, 0.0, 90.0);
	mRoundRectPath.lineTo(radius, 0);
	mRoundRectPath.arcTo(0, 0, radius, radius, 90.0, 90.0);
	mRoundRectPath.lineTo(0, height()-radius);
	mRoundRectPath.arcTo(0, height() - radius, radius, radius, 180.0, 90.0);
	mRoundRectPath.lineTo(width()-radius, height());
	mRoundRectPath.arcTo(width()-radius, height() -  radius, radius, radius, 270.0, 90.0);
	mRoundRectPath.closeSubpath();

	mShadowRectPath = mRoundRectPath.translated(100,100);
	



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


	painter->fillPath(mShadowRectPath, global_shadow_color);
	

	QColor btn_color(40,40,200,transp);

	if (m_mouse_over)
		btn_color = QColor(40,40,230,transp);


	painter->fillPath(mRoundRectPath, btn_color);


	//painter->setPen(QPen(QColor(100,100,100,230),  1, Qt::SolidLine));
	//painter->drawRect(boundingRect());
	
	//==================================================
	QFont  font("Courier New", 350);
	font.setWeight(QFont::Bold);
	painter->setFont(font);

	QFontMetrics metrics = QFontMetrics(font);
	int border = qMax(4, metrics.leading());

	QRect rect(0, 0, width() , height());
	painter->setPen(QColor(255, 255, 255, 110));

	painter->drawText((width() - rect.width())/2, border,
		rect.width(), rect.height(),
		Qt::AlignCenter | Qt::TextWordWrap, mText);
}

