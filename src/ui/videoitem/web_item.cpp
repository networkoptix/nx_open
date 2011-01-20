#include "web_item.h"
#include <QGraphicsProxyWidget>

CLWebItem::CLWebItem(GraphicsView* view, int max_width, int max_height, QString name):
CLAbstractSceneItem(view, max_width, max_height, name)
{
	mWeb_item = new QGraphicsProxyWidget(this);

	int width = 1200;
	m_web_widget.resize(width, width*3/4);

	mWeb_item->setWidget(&m_web_widget);

	qreal scale = (qreal)max_width/width;

	mWeb_item->scale(scale, scale);

	addSubItem(CloseSubItem);

	addToEevntTransparetList(mWeb_item);
}

CLWebItem::~CLWebItem()
{

}

void CLWebItem::navigate(const QString& str)
{
	m_web_widget.load(QUrl(str));
	//m_web_widget.load(QUrl("http://google.com"));
}

void CLWebItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{

}

QPointF CLWebItem::getBestSubItemPos(CLSubItemType type)
{
	switch(type)
	{
	case CloseSubItem:
		return QPointF(width()-270, 20);

	default:
		return QPointF(-1001, -1001);
		break;
	}

}