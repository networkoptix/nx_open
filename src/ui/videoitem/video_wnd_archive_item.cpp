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
	mArchiveNavigator->setVisible(false);
    addSubItem(mArchiveNavigator);
    onResize();
}

CLVideoWindowArchiveItem::~CLVideoWindowArchiveItem()
{
	mArchiveNavigator->goToFullScreenMode(false);
}

void CLVideoWindowArchiveItem::onResize()
{
    CLVideoWindowItem::onResize();
    if (isFullScreen())
    {
        mArchiveNavigator->goToFullScreenMode(false);
        mArchiveNavigator->goToFullScreenMode(true);
    }
}

QPointF CLVideoWindowArchiveItem::getBestSubItemPos(CLSubItemType type)
{
    if (type==ArchiveNavigatorSubItem)
        return QPointF(0, height() - m_archNavigatorHeight);

    return CLVideoWindowItem::getBestSubItemPos(type);
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

	if (full)
	{
        removeSubItem(mArchiveNavigator);
		mArchiveNavigator->goToFullScreenMode(true);
	}
	else
	{
		addSubItem(mArchiveNavigator);
        mArchiveNavigator->goToFullScreenMode(false);
	}

    CLVideoWindowItem::setFullScreen(full);

}

void CLVideoWindowArchiveItem::setComplicatedItem(CLAbstractComplicatedItem* complicatedItem)
{
	CLVideoWindowItem::setComplicatedItem(complicatedItem);
	mArchiveNavigator->setVideoCamera(getVideoCam());

}

