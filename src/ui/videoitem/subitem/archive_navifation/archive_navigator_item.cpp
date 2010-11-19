#include "archive_navigator_item.h"
#include "..\..\abstract_scene_item.h"
#include "..\abstract_image_sub_item.h"

#include <QPainter>
#include <QGraphicsProxyWidget>
#include "slider_item.h"


int NavigatorItemHeight = 200;

CLArchiveNavigatorItem::CLArchiveNavigatorItem(CLAbstractSubItemContainer* parent):
CLAbstractSubItem(parent, 0.2, 0.8)
{
	m_height = NavigatorItemHeight;
	m_width = parent->boundingRect().width();
	mType = ArchiveNavigator;

	/*/
	mPlayItem = new CLImgSubItem(this, "./skin/try/play2.png", CLAbstractSubItem::Play, 0.7, 1.0, NavigatorItemHeight, NavigatorItemHeight);
	mPauseItem = new CLImgSubItem(this, "./skin/try/pause2.png", CLAbstractSubItem::Pause, 0.7, 1.0, NavigatorItemHeight, NavigatorItemHeight);
	mPauseItem->setVisible(false);

	mSlider_item = new QGraphicsProxyWidget(this);
	mSlider = new CLDirectJumpSlider(Qt::Horizontal);
	mSlider->setRange(0,2000);


	mSlider->setStyleSheet("QSlider { height: 120px}"
	"QSlider::groove:horizontal {"
	"border: 8px solid #0f50af;"
	"background: qlineargradient(x1:0, y1:0, x2:0, y2:4, stop:0 #000030, stop:1 #0000af);"
	"top: 90px;"
	"height: 104px;"
	"margin: 0 0 0 0;}"
	"QSlider::handle:horizontal {"
	"background: qlineargradient(x1:0, y1:0, x2:4, y2:4, stop:0 #0000a0, stop:1 #3358e0);"
	"border: 10px solid #007eff;" // border of handle 
	"width: 120px;"
	"margin: -8px 0 -8px 0;"
	"border-radius: 30px;}");
	/**/


	/**/
	mPlayItem = new CLImgSubItem(this, "./skin/try/play1.png", CLAbstractSubItem::Play, 0.7, 1.0, NavigatorItemHeight, NavigatorItemHeight);
	mPauseItem = new CLImgSubItem(this, "./skin/try/pause1.png", CLAbstractSubItem::Pause, 0.7, 1.0, NavigatorItemHeight, NavigatorItemHeight);
	mPauseItem->setVisible(false);

	mSlider_item = new QGraphicsProxyWidget(this);
	mSlider = new CLDirectJumpSlider(Qt::Horizontal);
	mSlider->setRange(0,2000);


	mSlider->setStyleSheet("QSlider { height: 120px}"
		"QSlider::groove:horizontal {"
		"border: 8px solid #6a6a6a;"
		"background: qlineargradient(x1:0, y1:0, x2:0, y2:4, stop:0 #6a6a7a, stop:1 #6a6afa);"
		"top: 90px;"
		"height: 104px;"
		"margin: 0 0 0 0;}"
		"QSlider::handle:horizontal {"
		"background: qlineargradient(x1:0, y1:0, x2:4, y2:4, stop:0 #567ecd, stop:1 #567eff);"
		"border: 10px solid #205eff;" // border of handle 
		"width: 120px;"
		"margin: -8px 0 -8px 0;"
		"border-radius: 30px;}");

	/**/

	
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
