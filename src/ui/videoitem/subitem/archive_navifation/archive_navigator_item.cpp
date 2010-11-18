#include "archive_navigator_item.h"
#include "..\..\abstract_scene_item.h"
#include "..\abstract_image_sub_item.h"

#include <QPainter>
#include <QGraphicsProxyWidget>
#include <QSlider>

int NavigatorItemHeight = 200;

CLArchiveNavigatorItem::CLArchiveNavigatorItem(CLAbstractSubItemContainer* parent):
CLAbstractSubItem(parent, 0.2, 0.8)
{
	m_height = NavigatorItemHeight;
	m_width = parent->boundingRect().width();
	mType = ArchiveNavigator;

	mPlayItem = new CLImgSubItem(this, "./skin/try/play2.png", CLAbstractSubItem::Play, 0.7, 1.0, NavigatorItemHeight, NavigatorItemHeight);
	mPauseItem = new CLImgSubItem(this, "./skin/try/pause2.png", CLAbstractSubItem::Pause, 0.7, 1.0, NavigatorItemHeight, NavigatorItemHeight);
	mPauseItem->setVisible(false);

	mSlider_item = new QGraphicsProxyWidget(this);
	mSlider = new QSlider(Qt::Horizontal);
	mSlider->setRange(0,2000);
	//mSlider_item->scale(1.0, 5);

	/*
	mSlider->setStyleSheet("QSlider { height: 30px}"
		"QSlider::groove:horizontal {"
		"border: 1px solid #999999;"
		"background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #B1B1B1, stop:1 #c4c4c4);"
		"height: 26px;"
		"margin: 0 0 0 0;}"
		"QSlider::handle:horizontal {"
		"background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #b4b4b4, stop:1 #8f8f8f);"
		"border: 1px solid #5c5c5c;"
		"width: 30px;"
		"margin: -2px 0 -2px 0;"
		"border-radius: 3px;}");
		/**/


	mSlider->setStyleSheet("QSlider { height: 150px}"
		"QSlider::groove:horizontal {"
		"border: 5px solid #999999;"
		"background: qlineargradient(x1:0, y1:0, x2:0, y2:5, stop:0 #B1B1B1, stop:1 #c4c4c4);"
		"height: 130px;"
		"margin: 0 0 0 0;}"
		"QSlider::handle:horizontal {"
		"background: qlineargradient(x1:0, y1:0, x2:5, y2:5, stop:0 #b4b4b4, stop:1 #8f8f8f);"
		"border: 5px solid #5c5c5c;"
		"width: 150px;"
		"margin: -5px 0 -5px 0;"
		"border-radius: 15px;}");

	
	mSlider_item->setWidget(mSlider);
	
	onResize();
}

CLArchiveNavigatorItem::~CLArchiveNavigatorItem()
{

}

// this function uses parent width
void CLArchiveNavigatorItem::onResize()
{
	m_width = parentItem()->boundingRect().width();
	mPauseItem->setPos(5,0);
	mPlayItem->setPos(5,0);


	int slider_width = m_width - NavigatorItemHeight - 300;

	mSlider->resize(slider_width, 30);
	mSlider_item->setPos(m_width - slider_width - 50, 50);
	

}

void CLArchiveNavigatorItem::onSubItemPressed(CLAbstractSubItem* subitem)
{
	CLAbstractSubItem::ItemType type = subitem->getType();

	switch(type)
	{
	case CLAbstractSubItem::Play:
		mPlayItem->setVisible(false);
		mPauseItem->setVisible(true);
		break;

	case CLAbstractSubItem::Pause:
		mPlayItem->setVisible(true);
		mPauseItem->setVisible(false);
		break;


	default:
		break;
	}

}

void CLArchiveNavigatorItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
	painter->fillRect(boundingRect(),QColor(0, 0, 0));
}

QRectF CLArchiveNavigatorItem::boundingRect() const
{
	return QRectF(0,0,m_width, m_height);
}
