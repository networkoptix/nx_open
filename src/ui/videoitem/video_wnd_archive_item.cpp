#include "video_wnd_archive_item.h"
#include "subitem\archive_navifation\archive_navigator_item.h"

extern int NavigatorItemHeight;


CLVideoWindowArchiveItem::CLVideoWindowArchiveItem (GraphicsView* view, const CLDeviceVideoLayout* layout, 
													int max_width, int max_height, QString name):
CLVideoWindowItem(view, layout, max_width, max_height, name)
{
	mArchiveNavigator = new CLArchiveNavigatorItem(this);
	onResize();
}

CLVideoWindowArchiveItem::~CLVideoWindowArchiveItem()
{

}


QPointF CLVideoWindowArchiveItem::getBestSubItemPos(CLAbstractSubItem::ItemType type)
{
	QPointF result = CLVideoWindowItem::getBestSubItemPos(type);
	if (result.x()>=-1000 || result.y()>=-1000)
		return result;

	if (type==CLAbstractSubItem::ArchiveNavigator)
		return QPointF(0, height() - NavigatorItemHeight);
}

void CLVideoWindowArchiveItem::draw(CLVideoDecoderOutput& image, unsigned int channel)
{
	CLVideoWindowItem::draw(image, channel);
	mArchiveNavigator->updateSliderPos();
}