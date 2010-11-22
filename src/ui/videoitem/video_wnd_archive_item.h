#ifndef video_wnd_archive_h1753
#define video_wnd_archive_h1753

#include "video_wnd_item.h"
class CLArchiveNavigatorItem;

class CLVideoWindowArchiveItem : public CLVideoWindowItem
{
public:
	CLVideoWindowArchiveItem (GraphicsView* view, const CLDeviceVideoLayout* layout, int max_width, int max_height,
		QString name="");
	virtual ~CLVideoWindowArchiveItem();

	void draw(CLVideoDecoderOutput& image, unsigned int channel);

	QPointF getBestSubItemPos(CLAbstractSubItem::ItemType type);

protected:
	CLArchiveNavigatorItem* mArchiveNavigator;
};


#endif //video_wnd_archive_h1753