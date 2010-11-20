#ifndef video_wnd_archive_h1753
#define video_wnd_archive_h1753

#include "video_wnd_item.h"

class CLVideoWindowArchiveItem : public CLVideoWindowItem
{
public:
	CLVideoWindowArchiveItem (GraphicsView* view, const CLDeviceVideoLayout* layout, int max_width, int max_height,
		QString name="");
	virtual ~CLVideoWindowArchiveItem();

	QPointF getBestSubItemPos(CLAbstractSubItem::ItemType type);

protected:
};


#endif //video_wnd_archive_h1753