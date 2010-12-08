#include "search_filter_item.h"

#include <QLineEdit>
#include <QComboBox>
#include <QGraphicsProxyWidget>
#include <QGraphicsView>
#include <QCompleter>
#include <QTableView>

CLSerachEditItem::CLSerachEditItem(QString name, QGraphicsItem* parent, qreal normal_opacity, qreal active_opacity):
CLUnMovedOpacityItem(name, parent, normal_opacity, active_opacity),
m_width(100),
m_height(100)
{
	m_lineEdit = new QComboBox(0);
	m_lineEdit->setEditable(true);


	m_lineEditItem = new QGraphicsProxyWidget(this);
	m_lineEditItem->setWidget(m_lineEdit);
	m_lineEditItem->setPos(0,0);


	resize();
}

CLSerachEditItem::~CLSerachEditItem()
{

}



void CLSerachEditItem::resize()
{
	int vpw = 800;
	if (scene())
		vpw = scene()->views().at(0)->viewport()->width();

	m_width = vpw/5;
	if (m_width < 200)
		m_width  = 200;

	m_height = m_lineEdit->size().height();


	m_lineEdit->resize(m_width, m_height);

	setStaticPos(QPoint(vpw/2 - m_width/2, 0));
}

QRectF CLSerachEditItem::boundingRect() const
{
	return QRectF(0,0,m_width, m_height);
}


void CLSerachEditItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
	return;
}