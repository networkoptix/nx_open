#include "recorder_item.h"
#include "settings.h"
#include "ui/ui_common.h"
#include "ui/style/globals.h"
#include "recorder/recorder_display.h"

CLRecorderItem::CLRecorderItem(GraphicsView* view, int max_width, int max_height, QString name, QString tooltip)
	: CLCustomBtnItem(view,max_width,max_height, name, QString(), tooltip),
	  mContent(0)
{
	m_type = RECORDER;

	createPaths(width()/3);
}

CLRecorderItem::~CLRecorderItem()
{

}

void CLRecorderItem::setText(QString text)
{
	QMutexLocker locker(&m_mutex);
	mText = text;
	needUpdate(true);
}

CLRecorderDisplay* CLRecorderItem::getRecorderDisplay() const
{
	return static_cast<CLRecorderDisplay*>(m_complicatedItem);
}

QString CLRecorderItem::text() const
{
	QMutexLocker locker(&m_mutex);
	return mText;
}

void CLRecorderItem::setRefContent(LayoutContent* cont)
{
	mContent = cont;
}

LayoutContent* CLRecorderItem::getRefContent() const
{
	return mContent;
}

QPointF CLRecorderItem::getBestSubItemPos(CLSubItemType type)
{
	switch(type)
	{
	case CloseSubItem:
		return QPointF(width()-1000, 200);
		break;

	default:
		return QPointF(-1001, -1001);
		break;
	}

}

void CLRecorderItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
	Q_UNUSED(widget);

	painter->fillPath(mShadowRectPath, Globals::shadowColor());

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
	QFont  font(QLatin1String("Courier New"), 250);
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
		Qt::AlignCenter | Qt::TextWordWrap, text());

	if (option->state & QStyle::State_Selected)
	{
		painter->fillPath(mRoundRectPath, m_can_be_droped ? Globals::canBeDroppedColor() : Globals::selectionColor() );
	}

}
