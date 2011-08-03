#ifndef video_wnd_archive_h1753
#define video_wnd_archive_h1753

#include "video_wnd_item.h"
class CLArchiveNavigatorItem;
class CLAbstractArchiveReader;

class CLVideoWindowArchiveItem : public CLVideoWindowItem
{
public:
<<<<<<< local
    CLVideoWindowArchiveItem(GraphicsView* view, const CLDeviceVideoLayout* layout, int max_width, int max_height,
                             const QString &name = QString());
=======
	CLVideoWindowArchiveItem (GraphicsView* view, const CLDeviceVideoLayout* layout, int max_width, int max_height,
							  QString name = QString());
	virtual ~CLVideoWindowArchiveItem();
>>>>>>> other

    virtual void setItemSelected(bool sel, bool animate = true, int delay = 0);

<<<<<<< local
    NavigationItem *navigationItem() const { return m_navigationItem; }
    void setNavigationItem(NavigationItem *item) { m_navigationItem = item; }
=======
	virtual void setItemSelected(bool sel, bool animate = true, int delay = 0);

	void draw(CLVideoDecoderOutput& image, unsigned int channel);
	QPointF getBestSubItemPos(CLSubItemType  type);

	virtual void setComplicatedItem(CLAbstractComplicatedItem* complicatedItem);

	virtual void goToSteadyMode(bool steady, bool instant);

public slots:
	void onResize();

>>>>>>> other

protected:
    NavigationItem *m_navigationItem;
};

#endif //video_wnd_archive_h1753
