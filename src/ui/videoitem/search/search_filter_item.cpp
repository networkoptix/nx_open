#include "search_filter_item.h"

#include <QLineEdit>
#include <QGraphicsProxyWidget>
#include <QGraphicsView>
#include <QCompleter>
#include "search_edit.h"
#include "ui\graphicsview.h"
#include "device\device_managmen\device_criteria.h"
#include "ui\video_cam_layout\layout_content.h"


CLSerachEditItem::CLSerachEditItem(QString name, QGraphicsItem* parent, qreal normal_opacity, qreal active_opacity):
CLUnMovedOpacityItem(name, parent, normal_opacity, active_opacity),
m_width(100),
m_height(100)
{
	m_lineEdit = new CLSearchComboBox(0);
	m_lineEdit->setEditable(true);

	connect(m_lineEdit, SIGNAL(onTextChanged(QString)), this, SLOT(onTextChanged(QString)) );


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

void CLSerachEditItem::onTextChanged(QString text)
{

	GraphicsView * view = static_cast<GraphicsView*>(scene()->views().at(0));
	CLDeviceCriteria cr = view->getCamLayOut().getContent()->getDeviceCriteria();
	cr.setCriteria(CLDeviceCriteria::FILTER);
	cr.setFilter(text);

	QStringList result;
	
	CLDeviceList all_devs =  CLDeviceManager::instance().getDeviceList(cr);
	foreach(CLDevice* dev, all_devs)
	{
		result << dev->toString();
		dev->releaseRef();
	}

	
	m_lineEdit->clear();


	foreach(QString str, result)
	{
		m_lineEdit->addItem(str);
	}

	m_lineEdit->setEditText(text);
	m_lineEdit->lineEdit()->setFocus();


	//m_lineEdit->showPopup();

	view->getCamLayOut().getContent()->setDeviceCriteria(cr);

}