#ifndef video_wnd_archive_h1753
#define video_wnd_archive_h1753

#include "video_wnd_item.h"
class CLArchiveNavigatorItem;
class CLAbstractArchiveReader;

class CLVideoWindowArchiveItem : public CLVideoWindowItem
{
public:
	CLVideoWindowArchiveItem (GraphicsView* view, const CLDeviceVideoLayout* layout, int max_width, int max_height,
		QString name="");
	virtual ~CLVideoWindowArchiveItem();

	virtual void setFullScreen(bool full);

	virtual void setItemSelected(bool sel, bool animate = true, int delay = 0);

	void draw(CLVideoDecoderOutput& image, unsigned int channel);
	QPointF getBestSubItemPos(CLSubItemType  type);

	virtual void setComplicatedItem(CLAbstractComplicatedItem* complicatedItem);

public slots:
    void onResize();


protected:
	void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = 0);
protected:
	CLArchiveNavigatorItem* mArchiveNavigator;

	int m_archNavigatorHeight;
};

#endif //video_wnd_archive_h1753