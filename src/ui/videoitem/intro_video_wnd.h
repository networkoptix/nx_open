#ifndef video_wnd_intro_h1747
#define video_wnd_intro_h1747

#include "video_wnd_item.h"

class CLIntroVideoitem: public CLVideoWindowItem
{
public:
	CLIntroVideoitem(GraphicsView* view, const CLDeviceVideoLayout* layout, int max_width, int max_height,
		QString name="");

	virtual ~CLIntroVideoitem();

    void drawStuff(QPainter* painter);

    void goToSteadyMode(bool steady, bool instant);

};

#endif //video_wnd_archive_h1753
