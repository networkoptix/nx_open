#include "video_wnd_archive_item.h"
#include "subitem\archive_navifation\archive_navigator_item.h"
#include "base\log.h"
#include "camera\camera.h"
#include "device_plugins\archive\abstract_archive_stream_reader.h"



CLVideoWindowArchiveItem::CLVideoWindowArchiveItem (GraphicsView* view, const CLDeviceVideoLayout* layout, 
													int max_width, int max_height, QString name):
CLVideoWindowItem(view, layout, max_width, max_height, name)
{

	m_archNavigatorHeight = 400 ;

	mArchiveNavigator = new CLArchiveNavigatorItem(this, m_archNavigatorHeight);
	onResize();

	mArchiveNavigator->setVisible(false);
}

CLVideoWindowArchiveItem::~CLVideoWindowArchiveItem()
{
	mArchiveNavigator->goToFullScreenMode(false);
}


QPointF CLVideoWindowArchiveItem::getBestSubItemPos(CLSubItemType type)
{
	QPointF result = CLVideoWindowItem::getBestSubItemPos(type);
	if (result.x()>=-1000 || result.y()>=-1000)
		return result;

	if (type==ArchiveNavigatorSubItem)
		return QPointF(0, height() - m_archNavigatorHeight);
}

void CLVideoWindowArchiveItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
	mArchiveNavigator->updateSliderPos();
	CLVideoWindowItem::paint(painter, option, widget);
}

void CLVideoWindowArchiveItem::draw(CLVideoDecoderOutput& image, unsigned int channel)
{
	CLVideoWindowItem::draw(image, channel);
	
}


void CLVideoWindowArchiveItem::setItemSelected(bool sel, bool animate, int delay)
{
	CLVideoWindowItem::setItemSelected(sel, animate , delay );

	if (sel)
		mArchiveNavigator->setVisible(true);
	else
		mArchiveNavigator->setVisible(false);
}


void CLVideoWindowArchiveItem::setFullScreen(bool full)
{
	CLVideoWindowItem::setFullScreen(full);

	
	if (full)
	{
		/*
		mArchiveNavigator->setParentItem(0);
		scene()->addItem(mArchiveNavigator);
		mArchiveNavigator->onResize();
		/**/

		mArchiveNavigator->goToFullScreenMode(true);

		//cl_log.log("setFullScreen TRUE", cl_logALWAYS);
	}
	else
	{
		/*
		scene()->removeItem(mArchiveNavigator);
		mArchiveNavigator->setParentItem(this);
		mArchiveNavigator->onResize();
		/**/

		mArchiveNavigator->goToFullScreenMode(false);

		//cl_log.log("setFullScreen FALSE", cl_logALWAYS);
	}

	

}

void CLVideoWindowArchiveItem::setComplicatedItem(CLAbstractComplicatedItem* complicatedItem)
{
	CLVideoWindowItem::setComplicatedItem(complicatedItem);
	mArchiveNavigator->setVideoCamera(getVideoCam());

}

