#include <QPainter>
#include "settings.h"
#include <QMutexLocker>
#include <QStyleOptionGraphicsItem>
#include "layout_item.h"
#include "ui\ui_common.h"

CLLayoutItem::CLLayoutItem(GraphicsView* view, int max_width, int max_height, QString name, QString tooltip):
CLCustomBtnItem(view,max_width,max_height, name, "", tooltip),
mContent(0)
{
	m_type = LAYOUT;
}


CLLayoutItem::~CLLayoutItem()
{

}



void CLLayoutItem::setRefContent(LayoutContent* cont)
{
	mContent = cont;
}

LayoutContent* CLLayoutItem::getRefContent() const
{
	return mContent;
}


void CLLayoutItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
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
	QFont  font("Courier New", 250);
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
		Qt::AlignCenter | Qt::TextWordWrap, UIDisplayName(getName()));

	if (option->state & QStyle::State_Selected)
	{
		painter->fillPath(mRoundRectPath, global_selection_color );
	}


}
