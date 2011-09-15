#include "layout_item.h"

#include "settings.h"
#include "ui/ui_common.h"
#include "ui/skin.h"

CLLayoutItem::CLLayoutItem(GraphicsView* view, int max_width, int max_height, QString name, QString tooltip):
CLImageItem(view,max_width,max_height, name),
mContent(0)
{
    Q_UNUSED(tooltip);

    m_type = LAYOUT;

    mPixmap = cached(Skin::path(QLatin1String("layout.png")));

    //setMaxSize(max_width, max_height);
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

QPointF CLLayoutItem::getBestSubItemPos(CLSubItemType type)
{
	switch(type)
	{
	case CloseSubItem:
		return QPointF(width() - 350, 1670);
		break;

	default:
		return QPointF(-1001, -1001);
		break;
	}

}

void CLLayoutItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
	Q_UNUSED(widget);

	/*
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
    */

    painter->setRenderHint(QPainter::SmoothPixmapTransform);
    painter->setRenderHint(QPainter::Antialiasing);
    painter->drawPixmap(boundingRect(), mPixmap, mPixmap.rect());


	QFont font(QLatin1String("Courier New"), 250);
	font.setWeight(QFont::Bold);
	painter->setFont(font);

	QFontMetrics metrics = QFontMetrics(font);
	int border = qMax(4, metrics.leading());

	QRect rect(0, 0, width() , height());

    /*
	if (m_mouse_over)
		painter->setPen(QColor(190, 190, 255));
	else
		painter->setPen(QColor(150, 150, 255));
    /**/

    if (m_mouse_over)
        painter->setPen(QColor(0, 0, 0));
    else
        painter->setPen(QColor(30, 30, 130));



	painter->drawText((width() - rect.width())/2, border,
		rect.width(), rect.height(),
		Qt::AlignCenter | Qt::TextWordWrap, UIDisplayName(getName()));

	if (option->state & QStyle::State_Selected)
	{
		painter->fillRect(boundingRect(), m_can_be_droped ? global_can_be_droped_color :  global_selection_color );
	}

}
